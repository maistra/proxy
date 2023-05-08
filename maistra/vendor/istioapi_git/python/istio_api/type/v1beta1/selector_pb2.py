# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: type/v1beta1/selector.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()


from google.api import field_behavior_pb2 as google_dot_api_dot_field__behavior__pb2


DESCRIPTOR = _descriptor.FileDescriptor(
  name='type/v1beta1/selector.proto',
  package='istio.type.v1beta1',
  syntax='proto3',
  serialized_options=_b('Z\031istio.io/api/type/v1beta1'),
  serialized_pb=_b('\n\x1btype/v1beta1/selector.proto\x12\x12istio.type.v1beta1\x1a\x1fgoogle/api/field_behavior.proto\"\xb2\x01\n\x10WorkloadSelector\x12^\n\x0cmatch_labels\x18\x01 \x03(\x0b\x32\x35.istio.type.v1beta1.WorkloadSelector.MatchLabelsEntryB\x04\xe2\x41\x01\x02R\x0bmatchLabels\x1a>\n\x10MatchLabelsEntry\x12\x10\n\x03key\x18\x01 \x01(\tR\x03key\x12\x14\n\x05value\x18\x02 \x01(\tR\x05value:\x02\x38\x01\x42\x1bZ\x19istio.io/api/type/v1beta1b\x06proto3')
  ,
  dependencies=[google_dot_api_dot_field__behavior__pb2.DESCRIPTOR,])




_WORKLOADSELECTOR_MATCHLABELSENTRY = _descriptor.Descriptor(
  name='MatchLabelsEntry',
  full_name='istio.type.v1beta1.WorkloadSelector.MatchLabelsEntry',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='key', full_name='istio.type.v1beta1.WorkloadSelector.MatchLabelsEntry.key', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, json_name='key', file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='value', full_name='istio.type.v1beta1.WorkloadSelector.MatchLabelsEntry.value', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, json_name='value', file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=_b('8\001'),
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=201,
  serialized_end=263,
)

_WORKLOADSELECTOR = _descriptor.Descriptor(
  name='WorkloadSelector',
  full_name='istio.type.v1beta1.WorkloadSelector',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='match_labels', full_name='istio.type.v1beta1.WorkloadSelector.match_labels', index=0,
      number=1, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=_b('\342A\001\002'), json_name='matchLabels', file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[_WORKLOADSELECTOR_MATCHLABELSENTRY, ],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=85,
  serialized_end=263,
)

_WORKLOADSELECTOR_MATCHLABELSENTRY.containing_type = _WORKLOADSELECTOR
_WORKLOADSELECTOR.fields_by_name['match_labels'].message_type = _WORKLOADSELECTOR_MATCHLABELSENTRY
DESCRIPTOR.message_types_by_name['WorkloadSelector'] = _WORKLOADSELECTOR
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

WorkloadSelector = _reflection.GeneratedProtocolMessageType('WorkloadSelector', (_message.Message,), {

  'MatchLabelsEntry' : _reflection.GeneratedProtocolMessageType('MatchLabelsEntry', (_message.Message,), {
    'DESCRIPTOR' : _WORKLOADSELECTOR_MATCHLABELSENTRY,
    '__module__' : 'type.v1beta1.selector_pb2'
    # @@protoc_insertion_point(class_scope:istio.type.v1beta1.WorkloadSelector.MatchLabelsEntry)
    })
  ,
  'DESCRIPTOR' : _WORKLOADSELECTOR,
  '__module__' : 'type.v1beta1.selector_pb2'
  # @@protoc_insertion_point(class_scope:istio.type.v1beta1.WorkloadSelector)
  })
_sym_db.RegisterMessage(WorkloadSelector)
_sym_db.RegisterMessage(WorkloadSelector.MatchLabelsEntry)


DESCRIPTOR._options = None
_WORKLOADSELECTOR_MATCHLABELSENTRY._options = None
_WORKLOADSELECTOR.fields_by_name['match_labels']._options = None
# @@protoc_insertion_point(module_scope)
