#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "WAVM/Emscripten/Emscripten.h"
#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Inline/Version.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/ObjectCache/ObjectCache.h"
#include "WAVM/Platform/File.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/VFS/SandboxFS.h"
#include "WAVM/WASI/WASI.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"
#include "wavm.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

// A resolver that generates a stub if an inner resolver does not resolve a name.
struct StubFallbackResolver : Resolver
{
	StubFallbackResolver(Resolver& inInnerResolver, Compartment* inCompartment)
	: innerResolver(inInnerResolver), compartment(inCompartment)
	{
	}

	virtual bool resolve(const std::string& moduleName,
						 const std::string& exportName,
						 IR::ExternType type,
						 Object*& outObject) override
	{
		if(innerResolver.resolve(moduleName, exportName, type, outObject)) { return true; }
		else
		{
			Log::printf(Log::debug,
						"Generating stub for %s.%s : %s\n",
						moduleName.c_str(),
						exportName.c_str(),
						asString(type).c_str());
			return generateStub(
				moduleName, exportName, type, outObject, compartment, StubFunctionBehavior::trap);
		}
	}

private:
	Resolver& innerResolver;
	GCPointer<Compartment> compartment;
};

static bool loadTextOrBinaryModule(const char* filename,
								   std::vector<U8>&& fileBytes,
								   const IR::FeatureSpec& featureSpec,
								   ModuleRef& outModule)
{
	// If the file starts with the WASM binary magic number, load it as a binary module.
	if(fileBytes.size() >= sizeof(WASM::magicNumber)
	   && !memcmp(fileBytes.data(), WASM::magicNumber, sizeof(WASM::magicNumber)))
	{
		WASM::LoadError loadError;
		if(Runtime::loadBinaryModule(
			   fileBytes.data(), fileBytes.size(), outModule, featureSpec, &loadError))
		{ return true; }
		else
		{
			Log::printf(Log::error,
						"Error loading WebAssembly binary file: %s\n",
						loadError.message.c_str());
			return false;
		}
	}
	else
	{
		// Make sure the WAST file is null terminated.
		fileBytes.push_back(0);

		// Parse the module text format to IR.
		std::vector<WAST::Error> parseErrors;
		IR::Module irModule(featureSpec);
		if(!WAST::parseModule(
			   (const char*)fileBytes.data(), fileBytes.size(), irModule, parseErrors))
		{
			Log::printf(Log::error, "Error parsing WebAssembly text file:\n");
			WAST::reportParseErrors(filename, (const char*)fileBytes.data(), parseErrors);
			return false;
		}

		// Compile the IR.
		outModule = Runtime::compileModule(irModule);

		return true;
	}
}

static bool loadPrecompiledModule(std::vector<U8>&& fileBytes,
								  const IR::FeatureSpec& featureSpec,
								  ModuleRef& outModule)
{
	IR::Module irModule(featureSpec);

	// Deserialize the module IR from the binary format.
	WASM::LoadError loadError;
	if(!WASM::loadBinaryModule(fileBytes.data(), fileBytes.size(), irModule, &loadError))
	{
		Log::printf(
			Log::error, "Error loading WebAssembly binary file: %s\n", loadError.message.c_str());
		return false;
	}

	// Check for a precompiled object section.
	const CustomSection* precompiledObjectSection = nullptr;
	for(const CustomSection& customSection : irModule.customSections)
	{
		if(customSection.name == "wavm.precompiled_object")
		{
			precompiledObjectSection = &customSection;
			break;
		}
	}
	if(!precompiledObjectSection)
	{
		Log::printf(Log::error, "Input file did not contain 'wavm.precompiled_object' section.\n");
		return false;
	}
	else
	{
		// Load the IR + precompiled object code as a runtime module.
		outModule = Runtime::loadPrecompiledModule(irModule, precompiledObjectSection->data);
		return true;
	}
}

static void reportLinkErrors(const LinkResult& linkResult)
{
	Log::printf(Log::error, "Failed to link module:\n");
	for(auto& missingImport : linkResult.missingImports)
	{
		Log::printf(Log::error,
					"Missing import: module=\"%s\" export=\"%s\" type=\"%s\"\n",
					missingImport.moduleName.c_str(),
					missingImport.exportName.c_str(),
					asString(missingImport.type).c_str());
	}
}

static bool isWASIModule(const IR::Module& irModule)
{
	for(const auto& import : irModule.functions.imports)
	{
		if(import.moduleName == "wasi_unstable") { return true; }
	}

	return false;
}

static bool isEmscriptenModule(const IR::Module& irModule)
{
	for(const IR::CustomSection& customSection : irModule.customSections)
	{
		if(customSection.name == "emscripten_metadata") { return true; }
	}

	return false;
}

static const char* getABIListHelpText()
{
	return "  none        No ABI: bare virtual metal.\n"
		   "  emscripten  Emscripten ABI, such as it is.\n"
		   "  wasi        WebAssembly System Interface ABI.\n";
}

void showRunHelp(Log::Category outputCategory)
{
	Log::printf(outputCategory,
				"Usage: wavm run [options] <program file> [program arguments]\n"
				"  <program file>        The WebAssembly module (.wast/.wasm) to run\n"
				"  [program arguments]   The arguments to pass to the WebAssembly function\n"
				"\n"
				"Options:\n"
				"  -f|--function name    Specify function name to run in module (default:main)\n"
				"  --precompiled         Use precompiled object code in program file\n"
				"  --enable <feature>    Enable the specified feature. See the list of supported\n"
				"                        features below.\n"
				"  --abi=<abi>           Specifies the ABI used by the WASM module. See the list\n"
				"                        of supported ABIs below. The default is to detect the\n"
				"                        ABI based on the module imports/exports.\n"
				"  --mount-root <dir>    Mounts <dir> as the WASI root directory\n"
				"  --wasi-trace=<level>  Sets the level of WASI tracing:\n"
				"                        - syscalls\n"
				"                        - syscalls-with-callstacks\n"
				"\n"
				"ABIs:\n"
				"%s"
				"\n"
				"Features:\n"
				"%s"
				"\n",
				getABIListHelpText(),
				getFeatureListHelpText());
}

template<Uptr numPrefixChars>
static bool stringStartsWith(const char* string, const char (&prefix)[numPrefixChars])
{
	return !strncmp(string, prefix, numPrefixChars - 1);
}

enum class ABI
{
	detect,
	bare,
	emscripten,
	wasi
};

struct State
{
	IR::FeatureSpec featureSpec{false};

	// Command-line options.
	const char* filename = nullptr;
	const char* functionName = nullptr;
	const char* rootMountPath = nullptr;
	std::vector<std::string> runArgs;
	ABI abi = ABI::detect;
	bool precompiled = false;
	WASI::SyscallTraceLevel wasiTraceLavel = WASI::SyscallTraceLevel::none;

	// Objects that need to be cleaned up before exiting.
	GCPointer<Compartment> compartment = createCompartment();
	std::shared_ptr<Emscripten::Instance> emscriptenInstance;
	std::shared_ptr<WASI::Process> wasiProcess;
	std::shared_ptr<VFS::FileSystem> sandboxFS;

	~State()
	{
		emscriptenInstance.reset();
		wasiProcess.reset();

		WAVM_ERROR_UNLESS(tryCollectCompartment(std::move(compartment)));
	}

	bool parseCommandLineAndEnvironment(char** argv)
	{
		char** nextArg = argv;
		while(*nextArg)
		{
			if(!strcmp(*nextArg, "--function") || !strcmp(*nextArg, "-f"))
			{
				if(!*++nextArg)
				{
					showRunHelp(Log::error);
					return false;
				}
				functionName = *nextArg;
			}
			else if(stringStartsWith(*nextArg, "--abi="))
			{
				if(abi != ABI::detect)
				{
					Log::printf(Log::error, "'--sys=' may only occur once on the command line.\n");
					return false;
				}

				const char* abiString = *nextArg + strlen("--abi=");
				if(!strcmp(abiString, "bare")) { abi = ABI::bare; }
				else if(!strcmp(abiString, "emscripten"))
				{
					abi = ABI::emscripten;
				}
				else if(!strcmp(abiString, "wasi"))
				{
					abi = ABI::wasi;
				}
				else
				{
					Log::printf(Log::error,
								"Unknown ABI '%s'. Supported ABIs:\n"
								"%s"
								"\n",
								abiString,
								getABIListHelpText());
					return false;
				}
			}
			else if(!strcmp(*nextArg, "--enable"))
			{
				++nextArg;
				if(!*nextArg)
				{
					Log::printf(Log::error, "Expected feature name following '--enable'.\n");
					return false;
				}

				if(!parseAndSetFeature(*nextArg, featureSpec, true))
				{
					Log::printf(Log::error,
								"Unknown feature '%s'. Supported features:\n"
								"%s"
								"\n",
								*nextArg,
								getFeatureListHelpText());
					return false;
				}
			}
			else if(!strcmp(*nextArg, "--precompiled"))
			{
				precompiled = true;
			}
			else if(!strcmp(*nextArg, "--mount-root"))
			{
				if(rootMountPath)
				{
					Log::printf(Log::error,
								"'--mount-root' may only occur once on the command line.\n");
					return false;
				}

				++nextArg;
				if(!*nextArg)
				{
					Log::printf(Log::error, "Expected path following '--mount-root'.\n");
					return false;
				}

				rootMountPath = *nextArg;
			}
			else if(stringStartsWith(*nextArg, "--wasi-trace="))
			{
				if(wasiTraceLavel != WASI::SyscallTraceLevel::none)
				{
					Log::printf(Log::error,
								"--wasi-trace=' may only occur once on the command line.\n");
					return false;
				}

				const char* levelString = *nextArg + strlen("--mount-root=");

				if(!strcmp(levelString, "syscalls"))
				{ wasiTraceLavel = WASI::SyscallTraceLevel::syscalls; }
				else if(!strcmp(levelString, "syscalls-with-callstacks"))
				{
					wasiTraceLavel = WASI::SyscallTraceLevel::syscallsWithCallstacks;
				}
				else
				{
					Log::printf(Log::error, "Invalid WASI trace level: %s\n", levelString);
					return false;
				}
			}
			else if((*nextArg)[0] != '-')
			{
				filename = *nextArg;
				++nextArg;
				break;
			}
			else
			{
				Log::printf(Log::error, "Unknown command-line argument: '%s'\n", *nextArg);
				return false;
			}

			++nextArg;
		}

		if(!filename)
		{
			showRunHelp(Log::error);
			return false;
		}

		while(*nextArg) { runArgs.push_back(*nextArg++); };

		// Check that the requested features are supported by the host CPU.
		switch(LLVMJIT::validateTarget(LLVMJIT::getHostTargetSpec(), featureSpec))
		{
		case LLVMJIT::TargetValidationResult::valid: break;

		case LLVMJIT::TargetValidationResult::unsupportedArchitecture:
			Log::printf(Log::error, "Host architecture is not supported by WAVM.");
			return false;
		case LLVMJIT::TargetValidationResult::x86CPUDoesNotSupportSSE41:
			Log::printf(Log::error,
						"Host X86 CPU does not support SSE 4.1, which"
						" WAVM requires for WebAssembly SIMD code.\n");
			return false;
		case LLVMJIT::TargetValidationResult::wavmDoesNotSupportSIMDOnArch:
			Log::printf(Log::error, "WAVM does not support SIMD on the host CPU architecture.\n");
			return false;

		case LLVMJIT::TargetValidationResult::invalidTargetSpec:
		default: WAVM_UNREACHABLE();
		};

		const char* objectCachePath
			= WAVM_SCOPED_DISABLE_SECURE_CRT_WARNINGS(getenv("WAVM_OBJECT_CACHE_DIR"));
		if(objectCachePath && *objectCachePath)
		{
			Uptr maxBytes = 1024 * 1024 * 1024;

			const char* maxMegabytesEnv
				= WAVM_SCOPED_DISABLE_SECURE_CRT_WARNINGS(getenv("WAVM_OBJECT_CACHE_MAX_MB"));
			if(maxMegabytesEnv && *maxMegabytesEnv)
			{
				int maxMegabytes = atoi(maxMegabytesEnv);
				if(maxMegabytes <= 0)
				{
					Log::printf(
						Log::error,
						"Invalid object cache size \"%s\". Expected an integer greater than 1.",
						maxMegabytesEnv);
					return false;
				}
				maxBytes = Uptr(maxMegabytes) * 1000000;
			}

			// Calculate a "code key" that identifies the code involved in compiling WebAssembly to
			// object code in the cache. If recompiling the module would produce different object
			// code, the code key should be different, and if recompiling the module would produce
			// the same object code, the code key should be the same.
			LLVMJIT::Version llvmjitVersion = LLVMJIT::getVersion();
			U64 codeKey = 0;
			codeKey = Hash<U64>()(llvmjitVersion.llvmMajor, codeKey);
			codeKey = Hash<U64>()(llvmjitVersion.llvmMinor, codeKey);
			codeKey = Hash<U64>()(llvmjitVersion.llvmPatch, codeKey);
			codeKey = Hash<U64>()(WAVM_VERSION_MAJOR, codeKey);
			codeKey = Hash<U64>()(WAVM_VERSION_MINOR, codeKey);
			codeKey = Hash<U64>()(WAVM_VERSION_PATCH, codeKey);

			// Initialize the object cache.
			std::shared_ptr<Runtime::ObjectCacheInterface> objectCache;
			ObjectCache::OpenResult openResult
				= ObjectCache::open(objectCachePath, maxBytes, codeKey, objectCache);
			switch(openResult)
			{
			case ObjectCache::OpenResult::doesNotExist:
				Log::printf(
					Log::error, "Object cache directory \"%s\" does not exist.\n", objectCachePath);
				return false;
			case ObjectCache::OpenResult::notDirectory:
				Log::printf(Log::error,
							"Object cache path \"%s\" does not refer to a directory.\n",
							objectCachePath);
				return false;
			case ObjectCache::OpenResult::notAccessible:
				Log::printf(
					Log::error, "Object cache path \"%s\" is not accessible.\n", objectCachePath);
				return false;
			case ObjectCache::OpenResult::invalidDatabase:
				Log::printf(
					Log::error, "Object cache database in \"%s\" is not valid.\n", objectCachePath);
				return false;
			case ObjectCache::OpenResult::tooManyReaders:
				Log::printf(Log::error,
							"Object cache database in \"%s\" has too many concurrent readers.\n",
							objectCachePath);
				return false;

			case ObjectCache::OpenResult::success:
				Runtime::setGlobalObjectCache(std::move(objectCache));
				break;
			default: WAVM_UNREACHABLE();
			};
		}

		return true;
	}

	bool initABIEnvironment(const IR::Module& irModule)
	{
		// If the user didn't specify an ABI on the command-line, try to figure it out from the
		// module's imports.
		if(abi == ABI::detect)
		{
			if(isEmscriptenModule(irModule))
			{
				Log::printf(Log::debug, "Module appears to be an Emscripten module.\n");
				abi = ABI::emscripten;
			}
			else if(isWASIModule(irModule))
			{
				Log::printf(Log::debug, "Module appears to be a WASI module.\n");
				abi = ABI::wasi;
			}
			else
			{
				Log::printf(
					Log::output,
					"Warning: could not detect module ABI; using 'bare' ABI.\n"
					"  Note that WAVM only supports Emscripten modules that were compiled with the\n"
					"  '-s EMIT_EMSCRIPTEN_METADATA=1' flag.\n"
					"  You may suppress this warning by passing '--abi=bare' to wavm run.\n");
				abi = ABI::bare;
			}
		}

		// If a directory to mount as the root filesystem was passed on the command-line, create a
		// SandboxFS for it.
		if(rootMountPath)
		{
			if(abi != ABI::wasi)
			{
				Log::printf(Log::error, "--mount-root may only be used with the WASI ABI.\n");
				return false;
			}

			std::string absoluteRootMountPath;
			if(rootMountPath[0] == '/' || rootMountPath[0] == '\\' || rootMountPath[0] == '~'
			   || rootMountPath[1] == ':')
			{ absoluteRootMountPath = rootMountPath; }
			else
			{
				absoluteRootMountPath
					= Platform::getCurrentWorkingDirectory() + '/' + rootMountPath;
			}
			sandboxFS = VFS::makeSandboxFS(&Platform::getHostFS(), absoluteRootMountPath);
		}

		if(abi == ABI::emscripten)
		{
			// Instantiate the Emscripten environment.
			emscriptenInstance
				= Emscripten::instantiate(compartment,
										  Platform::getStdFD(Platform::StdDevice::in),
										  Platform::getStdFD(Platform::StdDevice::out),
										  Platform::getStdFD(Platform::StdDevice::err));
			if(!emscriptenInstance) { return false; }
		}
		else if(abi == ABI::wasi)
		{
			std::vector<std::string> args = runArgs;
			args.insert(args.begin(), "/proc/1/exe");

			// Create the WASI process.
			wasiProcess = WASI::createProcess(compartment,
											  std::move(args),
											  {},
											  sandboxFS.get(),
											  Platform::getStdFD(Platform::StdDevice::in),
											  Platform::getStdFD(Platform::StdDevice::out),
											  Platform::getStdFD(Platform::StdDevice::err));
		}

		if(wasiTraceLavel != WASI::SyscallTraceLevel::none)
		{
			if(abi != ABI::wasi)
			{
				Log::printf(Log::error, "--wasi-trace may only be used with the WASI ABI.\n");
				return false;
			}

			WASI::setSyscallTraceLevel(wasiTraceLavel);
		}

		return true;
	}

	I32 execute(const IR::Module& irModule, ModuleInstance* moduleInstance)
	{
		// Create a WASM execution context.
		Context* context = Runtime::createContext(compartment);

		// Call the module start function, if it has one.
		Function* startFunction = getStartFunction(moduleInstance);
		if(startFunction) { invokeFunction(context, startFunction); }

		if(emscriptenInstance)
		{
			// Initialize the Emscripten module instance.
			if(!Emscripten::initializeModuleInstance(
				   emscriptenInstance, context, irModule, moduleInstance))
			{ return EXIT_FAILURE; }
		}

		// Look up the function export to call, validate its type, and set up the invoke arguments.
		Function* function = nullptr;
		std::vector<Value> invokeArgs;
		if(functionName)
		{
			function = asFunctionNullable(getInstanceExport(moduleInstance, functionName));
			if(!function)
			{
				Log::printf(Log::error, "Module does not export '%s'\n", functionName);
				return EXIT_FAILURE;
			}

			const FunctionType functionType = getFunctionType(function);

			if(functionType.params().size() != runArgs.size())
			{
				Log::printf(Log::error,
							"'%s' expects %" WAVM_PRIuPTR
							" argument(s), but command line had %" WAVM_PRIuPTR ".\n",
							functionName,
							Uptr(functionType.params().size()),
							Uptr(runArgs.size()));
				return EXIT_FAILURE;
			}

			for(Uptr argIndex = 0; argIndex < runArgs.size(); ++argIndex)
			{
				const std::string& argString = runArgs[argIndex];
				Value value;
				switch(functionType.params()[argIndex])
				{
				case ValueType::i32: value = (U32)atoi(argString.c_str()); break;
				case ValueType::i64: value = (U64)atol(argString.c_str()); break;
				case ValueType::f32: value = (F32)atof(argString.c_str()); break;
				case ValueType::f64: value = atof(argString.c_str()); break;
				case ValueType::v128:
				case ValueType::anyref:
				case ValueType::funcref:
					Errors::fatalf("Cannot parse command-line argument for %s function parameter",
								   asString(functionType.params()[argIndex]));

				case ValueType::none:
				case ValueType::any:
				case ValueType::nullref:
				default: WAVM_UNREACHABLE();
				}
				invokeArgs.push_back(value);
			}
		}
		else if(abi == ABI::wasi)
		{
			// WASI just calls a _start function with the signature ()->().
			function = getTypedInstanceExport(moduleInstance, "_start", FunctionType());
			if(!function)
			{
				Log::printf(Log::error, "WASM module doesn't export WASI _start function.\n");
				return EXIT_FAILURE;
			}
		}
		else
		{
			// Emscripten calls main or _main with a signature (i32, i32)|() -> i32?
			function = asFunctionNullable(getInstanceExport(moduleInstance, "main"));
			if(!function)
			{ function = asFunctionNullable(getInstanceExport(moduleInstance, "_main")); }
			if(!function)
			{
				Log::printf(Log::error, "Module does not export main function\n");
				return EXIT_FAILURE;
			}
			const FunctionType functionType = getFunctionType(function);

			if(functionType.params().size() == 2)
			{
				if(!emscriptenInstance)
				{
					Log::printf(
						Log::error,
						"Module does not declare a default memory object to put arguments in.\n");
					return EXIT_FAILURE;
				}
				else
				{
					std::vector<std::string> args = runArgs;
					args.insert(args.begin(), filename);

					WAVM_ASSERT(emscriptenInstance);
					invokeArgs = Emscripten::injectCommandArgs(emscriptenInstance, context, args);
				}
			}
			else if(functionType.params().size() > 0)
			{
				Log::printf(Log::error,
							"WebAssembly function requires %" PRIu64
							" argument(s), but only 0 or 2 can be passed!",
							functionType.params().size());
				return EXIT_FAILURE;
			}
		}

		// Split the tagged argument values into their types and untagged values.
		std::vector<ValueType> invokeArgTypes;
		std::vector<UntaggedValue> untaggedInvokeArgs;
		for(const Value& arg : invokeArgs)
		{
			invokeArgTypes.push_back(arg.type);
			untaggedInvokeArgs.push_back(arg);
		}

		// Infer the expected type of the function from the number and type of the invoke's
		// arguments and the function's actual result types.
		const FunctionType invokeSig(getFunctionType(function).results(),
									 TypeTuple(invokeArgTypes));

		// Allocate an array to receive the invoke results.
		std::vector<UntaggedValue> untaggedInvokeResults;
		untaggedInvokeResults.resize(invokeSig.results().size());

		// Invoke the function.
		Timing::Timer executionTimer;
		invokeFunction(
			context, function, invokeSig, untaggedInvokeArgs.data(), untaggedInvokeResults.data());
		Timing::logTimer("Invoked function", executionTimer);

		if(untaggedInvokeResults.size() == 1 && invokeSig.results()[0] == ValueType::i32)
		{ return untaggedInvokeResults[0].i32; }
		else
		{
			// Convert the untagged result values to tagged values.
			std::vector<Value> invokeResults;
			invokeResults.resize(invokeSig.results().size());
			for(Uptr resultIndex = 0; resultIndex < untaggedInvokeResults.size(); ++resultIndex)
			{
				const ValueType resultType = invokeSig.results()[resultIndex];
				const UntaggedValue& untaggedResult = untaggedInvokeResults[resultIndex];
				invokeResults[resultIndex] = Value(resultType, untaggedResult);
			}

			Log::printf(
				Log::debug, "%s returned: %s\n", functionName, asString(invokeResults).c_str());

			return EXIT_SUCCESS;
		}
	}

	int run(char** argv)
	{
		// Parse the command line.
		if(!parseCommandLineAndEnvironment(argv)) { return EXIT_FAILURE; }

		// Read the specified file into a byte array.
		std::vector<U8> fileBytes;
		if(!loadFile(filename, fileBytes)) { return EXIT_FAILURE; }

		// Load the module from the byte array
		Runtime::ModuleRef module = nullptr;
		if(precompiled)
		{
			if(!loadPrecompiledModule(std::move(fileBytes), featureSpec, module))
			{ return EXIT_FAILURE; }
		}
		else if(!loadTextOrBinaryModule(filename, std::move(fileBytes), featureSpec, module))
		{
			return EXIT_FAILURE;
		}
		const IR::Module& irModule = Runtime::getModuleIR(module);

		// Initialize the ABI-specific environment.
		if(!initABIEnvironment(irModule)) { return EXIT_FAILURE; }

		// Link the module with the intrinsic modules.
		LinkResult linkResult;
		if(abi == ABI::emscripten)
		{
			StubFallbackResolver stubFallbackResolver(
				Emscripten::getInstanceResolver(emscriptenInstance), compartment);
			linkResult = linkModule(irModule, stubFallbackResolver);
		}
		else if(abi == ABI::wasi)
		{
			linkResult = linkModule(irModule, WASI::getProcessResolver(*wasiProcess));
		}
		else if(abi == ABI::bare)
		{
			NullResolver nullResolver;
			linkResult = linkModule(irModule, nullResolver);
		}
		else
		{
			WAVM_UNREACHABLE();
		}

		if(!linkResult.success)
		{
			reportLinkErrors(linkResult);
			return EXIT_FAILURE;
		}

		// Instantiate the module.
		ModuleInstance* moduleInstance = instantiateModule(
			compartment, module, std::move(linkResult.resolvedImports), filename);
		if(!moduleInstance) { return EXIT_FAILURE; }

		// Take the module's memory as the WASI process memory.
		if(abi == ABI::wasi)
		{
			Memory* memory = asMemoryNullable(getInstanceExport(moduleInstance, "memory"));
			if(!memory)
			{
				Log::printf(Log::error, "WASM module doesn't export WASI memory.\n");
				return EXIT_FAILURE;
			}
			WASI::setProcessMemory(*wasiProcess, memory);
		}

		// Execute the program.
		auto executeThunk = [&] { return execute(irModule, moduleInstance); };
		int result;
		if(emscriptenInstance) { result = Emscripten::catchExit(std::move(executeThunk)); }
		else if(wasiProcess)
		{
			result = WASI::catchExit(std::move(executeThunk));
		}
		else
		{
			result = executeThunk();
		}

		// Log the peak memory usage.
		Uptr peakMemoryUsage = Platform::getPeakMemoryUsageBytes();
		Log::printf(
			Log::metrics, "Peak memory usage: %" WAVM_PRIuPTR "KiB\n", peakMemoryUsage / 1024);

		return result;
	}

	int runAndCatchRuntimeExceptions(char** argv)
	{
		int result = EXIT_FAILURE;
		Runtime::catchRuntimeExceptions([&result, argv, this]() { result = run(argv); },
										[](Runtime::Exception* exception) {
											// Treat any unhandled exception as a fatal error.
											Errors::fatalf("Runtime exception: %s",
														   describeException(exception).c_str());
										});
		return result;
	}
};

int execRunCommand(int argc, char** argv)
{
	State state;
	return state.runAndCatchRuntimeExceptions(argv);
}