// Copyright 2024, The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Test data used in libavb_rs tests.
//!
//! These constants must match the values used to create the images in Android.bp.

pub const TEST_IMAGE_PATH: &str = "test_image.img";
pub const TEST_IMAGE_SIZE: usize = 16 * 1024;
pub const TEST_IMAGE_SALT_HEX: &str = "1000";
pub const TEST_HASHTREE_SALT_HEX: &str = "B000";
pub const TEST_VBMETA_PATH: &str = "test_vbmeta.img";
pub const TEST_VBMETA_2_PARTITIONS_PATH: &str = "test_vbmeta_2_parts.img";
pub const TEST_VBMETA_PERSISTENT_DIGEST_PATH: &str = "test_vbmeta_persistent_digest.img";
pub const TEST_VBMETA_WITH_PROPERTY_PATH: &str = "test_vbmeta_with_property.img";
pub const TEST_VBMETA_WITH_HASHTREE_PATH: &str = "test_vbmeta_with_hashtree.img";
pub const TEST_VBMETA_WITH_COMMANDLINE_PATH: &str = "test_vbmeta_with_commandline.img";
pub const TEST_VBMETA_WITH_CHAINED_PARTITION_PATH: &str = "test_vbmeta_with_chained_partition.img";
pub const TEST_IMAGE_WITH_VBMETA_FOOTER_PATH: &str = "avbrs_test_image_with_vbmeta_footer.img";
pub const TEST_IMAGE_WITH_VBMETA_FOOTER_FOR_BOOT_PATH: &str =
    "avbrs_test_image_with_vbmeta_footer_for_boot.img";
pub const TEST_IMAGE_WITH_VBMETA_FOOTER_FOR_TEST_PART_2: &str =
    "avbrs_test_image_with_vbmeta_footer_for_test_part_2.img";
pub const TEST_PUBLIC_KEY_PATH: &str = "data/testkey_rsa4096_pub.bin";
pub const TEST_PUBLIC_KEY_RSA8192_PATH: &str = "data/testkey_rsa8192_pub.bin";
pub const TEST_PARTITION_NAME: &str = "test_part";
pub const TEST_PARTITION_SLOT_C_NAME: &str = "test_part_c";
pub const TEST_PARTITION_2_NAME: &str = "test_part_2";
pub const TEST_PARTITION_PERSISTENT_DIGEST_NAME: &str = "test_part_persistent_digest";
pub const TEST_PARTITION_HASH_TREE_NAME: &str = "test_part_hashtree";
pub const TEST_VBMETA_ROLLBACK_LOCATION: usize = 0; // Default value, we don't explicitly set this.
pub const TEST_PROPERTY_KEY: &str = "test_prop_key";
pub const TEST_PROPERTY_VALUE: &[u8] = b"test_prop_value";
pub const TEST_KERNEL_COMMANDLINE: &str = "test_cmdline_key=test_cmdline_value";
pub const TEST_CHAINED_PARTITION_ROLLBACK_LOCATION: usize = 4;
pub const TEST_CHAINED_PARTITION_ROLLBACK_INDEX: u64 = 7;

// Expected values determined by examining the vbmeta image with `avbtool info_image`.
// Images can be found in <out>/soong/.intermediates/external/avb/rust/.
pub const TEST_IMAGE_DIGEST_HEX: &str =
    "89e6fd3142917b8c34ac7d30897a907a71bd3bf5d9b39d00bf938b41dcf3b84f";
pub const TEST_IMAGE_HASH_ALGO: &str = "sha256";
pub const TEST_HASHTREE_DIGEST_HEX: &str = "5373fc4ee3dd898325eeeffb5a1dbb041900c5f1";
pub const TEST_HASHTREE_ALGORITHM: &str = "sha1";

// Certificate test data.
pub const TEST_CERT_PERMANENT_ATTRIBUTES_PATH: &str = "data/cert_permanent_attributes.bin";
pub const TEST_CERT_VBMETA_PATH: &str = "test_vbmeta_cert.img";
pub const TEST_CERT_UNLOCK_CHALLENGE_RNG_PATH: &str = "data/cert_unlock_challenge.bin";
pub const TEST_CERT_UNLOCK_CREDENTIAL_PATH: &str = "data/cert_unlock_credential.bin";

// The cert test keys were both generated with rollback version 42.
pub const TEST_CERT_PIK_VERSION: u64 = 42;
pub const TEST_CERT_PSK_VERSION: u64 = 42;

// $ sha256sum external/avb/test/data/cert_permanent_attributes.bin
pub const TEST_CERT_PERMANENT_ATTRIBUTES_HASH_HEX: &str =
    "55419e1affff153b58f65ce8a5313a71d2a83a00d0abae10a25b9a8e493d04f7";

// $ sha256sum external/avb/test/data/cert_product_id.bin
pub const TEST_CERT_PRODUCT_ID_HASH_HEX: &str =
    "374708fff7719dd5979ec875d56cd2286f6d3cf7ec317a3b25632aab28ec37bb";
