// Copyright 2023, The Android Open Source Project
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

mod cert_tests;
mod test_data;
mod test_ops;
mod verify_tests;

use avb::{slot_verify, HashtreeErrorMode, SlotVerifyData, SlotVerifyFlags, SlotVerifyResult};
use std::{ffi::CString, fs};
use test_data::*;
use test_ops::{FakeVbmetaKey, TestOps};

/// Initializes a `TestOps` object such that verification will succeed on `TEST_PARTITION_NAME`.
///
/// This usually forms the basis of the `TestOps` objects used, with tests modifying the returned
/// object as needed for the individual test case.
fn build_test_ops_one_image_one_vbmeta<'a>() -> TestOps<'a> {
    let mut ops = TestOps::default();
    ops.add_partition(TEST_PARTITION_NAME, fs::read(TEST_IMAGE_PATH).unwrap());
    ops.add_partition("vbmeta", fs::read(TEST_VBMETA_PATH).unwrap());
    ops.default_vbmeta_key = Some(FakeVbmetaKey::Avb {
        public_key: fs::read(TEST_PUBLIC_KEY_PATH).unwrap(),
        public_key_metadata: None,
    });
    ops.rollbacks.insert(TEST_VBMETA_ROLLBACK_LOCATION, 0);
    ops.unlock_state = Ok(false);
    ops
}

/// Calls `slot_verify()` using standard args for `build_test_ops_one_image_one_vbmeta()` setup.
fn verify_one_image_one_vbmeta<'a>(
    ops: &mut TestOps<'a>,
) -> SlotVerifyResult<'a, SlotVerifyData<'a>> {
    slot_verify(
        ops,
        &[&CString::new(TEST_PARTITION_NAME).unwrap()],
        None,
        SlotVerifyFlags::AVB_SLOT_VERIFY_FLAGS_NONE,
        HashtreeErrorMode::AVB_HASHTREE_ERROR_MODE_EIO,
    )
}
