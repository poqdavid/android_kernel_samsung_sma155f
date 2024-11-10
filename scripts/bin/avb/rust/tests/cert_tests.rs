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

//! libavb_rs certificate tests.

use crate::{
    build_test_ops_one_image_one_vbmeta,
    test_data::*,
    test_ops::{FakeVbmetaKey, TestOps},
    verify_one_image_one_vbmeta,
};
use avb::{
    cert_generate_unlock_challenge, cert_validate_unlock_credential, CertPermanentAttributes,
    CertUnlockChallenge, CertUnlockCredential, IoError, SlotVerifyError, CERT_PIK_VERSION_LOCATION,
    CERT_PSK_VERSION_LOCATION,
};
use hex::decode;
use std::{collections::HashMap, fs, mem::size_of};
use zerocopy::{AsBytes, FromBytes};

/// Initializes a `TestOps` object such that cert verification will succeed on
/// `TEST_PARTITION_NAME`.
///
/// The returned `TestOps` also contains RNG configured to return the contents of
/// `TEST_CERT_UNLOCK_CHALLENGE_RNG_PATH`, so that the pre-signed contents of
/// `TEST_CERT_UNLOCK_CREDENTIAL_PATH` will successfully validate by default.
fn build_test_cert_ops_one_image_one_vbmeta<'a>() -> TestOps<'a> {
    let mut ops = build_test_ops_one_image_one_vbmeta();

    // Replace vbmeta with the cert-signed version.
    ops.add_partition("vbmeta", fs::read(TEST_CERT_VBMETA_PATH).unwrap());

    // Tell `ops` to use cert APIs and to route the default key through cert validation.
    ops.use_cert = true;
    ops.default_vbmeta_key = Some(FakeVbmetaKey::Cert);

    // Add the libavb_cert permanent attributes.
    let perm_attr_bytes = fs::read(TEST_CERT_PERMANENT_ATTRIBUTES_PATH).unwrap();
    ops.cert_permanent_attributes =
        Some(CertPermanentAttributes::read_from(&perm_attr_bytes[..]).unwrap());
    ops.cert_permanent_attributes_hash = Some(
        decode(TEST_CERT_PERMANENT_ATTRIBUTES_HASH_HEX)
            .unwrap()
            .try_into()
            .unwrap(),
    );

    // Add the rollbacks for the cert keys.
    ops.rollbacks
        .insert(CERT_PIK_VERSION_LOCATION, TEST_CERT_PIK_VERSION);
    ops.rollbacks
        .insert(CERT_PSK_VERSION_LOCATION, TEST_CERT_PSK_VERSION);

    // It's non-trivial to sign a challenge without `avbtool.py`, so instead we inject the exact RNG
    // used by the pre-generated challenge so that we can use the pre-signed credential.
    ops.cert_fake_rng = fs::read(TEST_CERT_UNLOCK_CHALLENGE_RNG_PATH).unwrap();

    ops
}

/// Returns the contents of `TEST_CERT_UNLOCK_CREDENTIAL_PATH` as a `CertUnlockCredential`.
fn test_unlock_credential() -> CertUnlockCredential {
    let credential_bytes = fs::read(TEST_CERT_UNLOCK_CREDENTIAL_PATH).unwrap();
    CertUnlockCredential::read_from(&credential_bytes[..]).unwrap()
}

/// Enough fake RNG data to generate a single unlock challenge.
const UNLOCK_CHALLENGE_FAKE_RNG: [u8; 16] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];

/// Returns a `TestOps` with only enough configuration to generate a single unlock challenge.
///
/// The generated unlock challenge will have:
/// * permanent attributes sourced from the contents of `TEST_CERT_PERMANENT_ATTRIBUTES_PATH`.
/// * RNG sourced from `UNLOCK_CHALLENGE_FAKE_RNG`.
fn build_test_cert_ops_unlock_challenge_only<'a>() -> TestOps<'a> {
    let mut ops = TestOps::default();

    // Permanent attributes are needed for the embedded product ID.
    let perm_attr_bytes = fs::read(TEST_CERT_PERMANENT_ATTRIBUTES_PATH).unwrap();
    ops.cert_permanent_attributes =
        Some(CertPermanentAttributes::read_from(&perm_attr_bytes[..]).unwrap());

    // Fake RNG for unlock challenge generation.
    ops.cert_fake_rng = UNLOCK_CHALLENGE_FAKE_RNG.into();

    ops
}

#[test]
fn cert_verify_succeeds() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();

    let result = verify_one_image_one_vbmeta(&mut ops);

    assert!(result.is_ok());
}

#[test]
fn cert_verify_sets_key_rollbacks() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();

    // `cert_key_versions` should start empty and be filled by the `set_key_version()` callback
    // during cert key validation.
    assert!(ops.cert_key_versions.is_empty());

    let result = verify_one_image_one_vbmeta(&mut ops);
    assert!(result.is_ok());

    assert_eq!(
        ops.cert_key_versions,
        HashMap::from([
            (CERT_PIK_VERSION_LOCATION, TEST_CERT_PIK_VERSION),
            (CERT_PSK_VERSION_LOCATION, TEST_CERT_PSK_VERSION)
        ])
    );
}

#[test]
fn cert_verify_fails_with_pik_rollback_violation() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();
    // If the image is signed with a lower key version than our rollback, it should fail to verify.
    *ops.rollbacks.get_mut(&CERT_PIK_VERSION_LOCATION).unwrap() += 1;

    let result = verify_one_image_one_vbmeta(&mut ops);

    assert_eq!(result.unwrap_err(), SlotVerifyError::PublicKeyRejected);
}

#[test]
fn cert_verify_fails_with_psk_rollback_violation() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();
    // If the image is signed with a lower key version than our rollback, it should fail to verify.
    *ops.rollbacks.get_mut(&CERT_PSK_VERSION_LOCATION).unwrap() += 1;

    let result = verify_one_image_one_vbmeta(&mut ops);

    assert_eq!(result.unwrap_err(), SlotVerifyError::PublicKeyRejected);
}

#[test]
fn cert_verify_fails_with_wrong_vbmeta_key() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();
    // The default non-cert vbmeta image should fail to verify.
    ops.add_partition("vbmeta", fs::read(TEST_VBMETA_PATH).unwrap());

    let result = verify_one_image_one_vbmeta(&mut ops);

    assert_eq!(result.unwrap_err(), SlotVerifyError::PublicKeyRejected);
}

#[test]
fn cert_verify_fails_with_bad_permanent_attributes_hash() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();
    // The permanent attributes must match their hash.
    ops.cert_permanent_attributes_hash.as_mut().unwrap()[0] ^= 0x01;

    let result = verify_one_image_one_vbmeta(&mut ops);

    assert_eq!(result.unwrap_err(), SlotVerifyError::PublicKeyRejected);
}

#[test]
fn cert_generate_unlock_challenge_succeeds() {
    let mut ops = build_test_cert_ops_unlock_challenge_only();

    let challenge = cert_generate_unlock_challenge(&mut ops).unwrap();

    // Make sure the challenge token used our cert callback data correctly.
    assert_eq!(
        challenge.product_id_hash,
        &decode(TEST_CERT_PRODUCT_ID_HASH_HEX).unwrap()[..]
    );
    assert_eq!(challenge.challenge, UNLOCK_CHALLENGE_FAKE_RNG);
}

#[test]
fn cert_generate_unlock_challenge_fails_without_permanent_attributes() {
    let mut ops = build_test_cert_ops_unlock_challenge_only();

    // Challenge generation should fail without the product ID provided by the permanent attributes.
    ops.cert_permanent_attributes = None;

    assert_eq!(
        cert_generate_unlock_challenge(&mut ops).unwrap_err(),
        IoError::Io
    );
}

#[test]
fn cert_generate_unlock_challenge_fails_insufficient_rng() {
    let mut ops = build_test_cert_ops_unlock_challenge_only();

    // Remove a byte of RNG so there isn't enough.
    ops.cert_fake_rng.pop();

    assert_eq!(
        cert_generate_unlock_challenge(&mut ops).unwrap_err(),
        IoError::Io
    );
}

#[test]
fn cert_validate_unlock_credential_success() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();

    // We don't actually need the challenge here since we've pre-signed it, but we still need to
    // call this function so the libavb_cert internal state is ready for the unlock cred.
    let _ = cert_generate_unlock_challenge(&mut ops).unwrap();

    assert_eq!(
        cert_validate_unlock_credential(&mut ops, &test_unlock_credential()),
        Ok(true)
    );
}

#[test]
fn cert_validate_unlock_credential_fails_wrong_rng() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();
    // Modify the RNG slightly, the cerificate should now fail to validate.
    ops.cert_fake_rng[0] ^= 0x01;

    let _ = cert_generate_unlock_challenge(&mut ops).unwrap();

    assert_eq!(
        cert_validate_unlock_credential(&mut ops, &test_unlock_credential()),
        Ok(false)
    );
}

#[test]
fn cert_validate_unlock_credential_fails_with_pik_rollback_violation() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();
    // Rotating the PIK should invalidate all existing unlock keys, which includes our pre-signed
    // certificate.
    *ops.rollbacks.get_mut(&CERT_PIK_VERSION_LOCATION).unwrap() += 1;

    let _ = cert_generate_unlock_challenge(&mut ops).unwrap();

    assert_eq!(
        cert_validate_unlock_credential(&mut ops, &test_unlock_credential()),
        Ok(false)
    );
}

#[test]
fn cert_validate_unlock_credential_fails_no_challenge() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();

    // We never called `cert_generate_unlock_challenge()`, so no credentials should validate.
    assert_eq!(
        cert_validate_unlock_credential(&mut ops, &test_unlock_credential()),
        Ok(false)
    );
}

// In practice, devices will usually be passing unlock challenges and credentials over fastboot as
// raw bytes. This test ensures that there are some reasonable APIs available to convert between
// `CertUnlockChallenge`/`CertUnlockCredential` and byte slices.
#[test]
fn cert_validate_unlock_credential_bytes_api() {
    let mut ops = build_test_cert_ops_one_image_one_vbmeta();

    // Write an unlock challenge to a byte buffer for TX over fastboot.
    let challenge = cert_generate_unlock_challenge(&mut ops).unwrap();
    let mut buffer = vec![0u8; size_of::<CertUnlockChallenge>()];
    assert_eq!(challenge.write_to(&mut buffer[..]), Some(())); // zerocopy::AsBytes.

    // Read an unlock credential from a byte buffer for RX from fastboot.
    let buffer = vec![0u8; size_of::<CertUnlockCredential>()];
    let credential = CertUnlockCredential::ref_from(&buffer[..]).unwrap(); // zerocopy::FromBytes.

    // It shouldn't actually validate since the credential is just zeroes, the important thing
    // is that it compiles.
    assert_eq!(
        cert_validate_unlock_credential(&mut ops, credential),
        Ok(false)
    );
}
