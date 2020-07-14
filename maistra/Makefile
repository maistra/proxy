.PHONY: maistra-update-envoy-sha maistra-update-deps maistra-update-everything

maistra-update-envoy-sha:
	@maistra/scripts/update-envoy-sha.sh

maistra-update-deps:
	@maistra/scripts/update-deps.sh

maistra-update-everything: maistra-update-envoy-sha maistra-update-deps
