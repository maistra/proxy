// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.#pragma once

#pragma once

#include <string>
#include <vector>

#include "bssl_wrapper/bssl_wrapper.h"

#define CBS_ASN1_TAG_SHIFT 24
#define CBS_ASN1_CONSTRUCTED (0x20u << CBS_ASN1_TAG_SHIFT)
#define CBS_ASN1_SEQUENCE (0x10u | CBS_ASN1_CONSTRUCTED)
#define CBS_ASN1_TAG_NUMBER_MASK ((1u << (5 + CBS_ASN1_TAG_SHIFT)) - 1)
#define CBS_ASN1_INTEGER 0x2u
#define CBS_ASN1_CONTEXT_SPECIFIC (0x80u << CBS_ASN1_TAG_SHIFT)

//namespace Openssl {
//namespace Cbs {

class CBS {
  public:
	CBS();
	CBS(const uint8_t *data, size_t len);
	const uint8_t *data_;
	size_t len_;
};

int BN_cmp_word(BIGNUM *a, BN_ULONG b);
int BN_parse_asn1_unsigned(CBS *cbs, BIGNUM *ret);

RSA* RSA_public_key_from_bytes(const uint8_t *in, size_t in_len);
RSA* RSA_parse_public_key(CBS *cbs);

int parse_asn1_tag(CBS *cbs, unsigned *out);
int parse_base128_integer(CBS *cbs, uint64_t *out);
int parse_integer(CBS *cbs, BIGNUM **out);

int cbs_get_asn1(CBS *cbs, CBS *out, unsigned tag_value, int skip_header);

int CBS_get_asn1(CBS *cbs, CBS *out, unsigned tag_value);
int CBS_skip(CBS *cbs, size_t len);
int CBS_get(CBS *cbs, const uint8_t **p, size_t n);
int CBS_get_any_asn1_element(CBS *cbs, CBS *out, unsigned *out_tag, size_t *out_header_len, int ber_ok);
int CBS_get_u(CBS *cbs, uint32_t *out, size_t len);
int CBS_get_bytes(CBS *cbs, CBS *out, size_t len);
int CBS_get_optional_asn1(CBS *cbs, CBS *out, int *out_present, unsigned tag);
int CBS_peek_asn1_tag(const CBS *cbs, unsigned tag_value);
size_t CBS_len(const CBS *cbs);
int CBS_get_asn1_element(CBS *cbs, CBS *out, unsigned tag_value);
const uint8_t *CBS_data(const CBS *cbs);
void CBS_init(CBS *cbs, const uint8_t *data, size_t len);
int CBS_get_u8(CBS *cbs, uint8_t *out);






//}  // namespace Cbs
//}  // namespace Openssl
