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

//! Provides `avb::Ops` test fixtures.

use avb::{
    cert_validate_vbmeta_public_key, CertOps, CertPermanentAttributes, IoError, IoResult, Ops,
    PublicKeyForPartitionInfo, SHA256_DIGEST_SIZE,
};
use std::{cmp::min, collections::HashMap, ffi::CStr};
#[cfg(feature = "uuid")]
use uuid::Uuid;

/// Where the fake partition contents come from.
pub enum PartitionContents<'a> {
    /// Read on-demand from disk.
    FromDisk(Vec<u8>),
    /// Preloaded and passed in.
    Preloaded(&'a [u8]),
}

impl<'a> PartitionContents<'a> {
    /// Returns the partition data.
    pub fn as_slice(&self) -> &[u8] {
        match self {
            Self::FromDisk(v) => v,
            Self::Preloaded(c) => c,
        }
    }

    /// Returns a mutable reference to the `FromDisk` data for test modification. Panicks if the
    /// data is actually `Preloaded` instead.
    pub fn as_mut_vec(&mut self) -> &mut Vec<u8> {
        match self {
            Self::FromDisk(v) => v,
            Self::Preloaded(_) => panic!("Cannot mutate preloaded partition data"),
        }
    }
}

/// Represents a single fake partition.
pub struct FakePartition<'a> {
    /// Partition contents, either preloaded or read on-demand.
    pub contents: PartitionContents<'a>,

    /// Partition UUID.
    #[cfg(feature = "uuid")]
    pub uuid: Uuid,
}

impl<'a> FakePartition<'a> {
    fn new(contents: PartitionContents<'a>) -> Self {
        Self {
            contents,
            #[cfg(feature = "uuid")]
            uuid: Default::default(),
        }
    }
}

/// Fake vbmeta key.
pub enum FakeVbmetaKey {
    /// Standard AVB validation using a hardcoded key; if the signing key matches these contents
    /// it is accepted, otherwise it's rejected.
    Avb {
        /// Expected public key contents.
        public_key: Vec<u8>,
        /// Expected public key metadata contents.
        public_key_metadata: Option<Vec<u8>>,
    },
    /// libavb_cert validation using the permanent attributes.
    Cert,
}

/// Fake `Ops` test fixture.
///
/// The user is expected to set up the internal values to the desired device state - disk contents,
/// rollback indices, etc. This class then uses this state to implement the avb callback operations.
pub struct TestOps<'a> {
    /// Partitions to provide to libavb callbacks.
    pub partitions: HashMap<&'static str, FakePartition<'a>>,

    /// Default vbmeta key to use for the `validate_vbmeta_public_key()` callback, or `None` to
    /// return `IoError::Io` when accessing this key.
    pub default_vbmeta_key: Option<FakeVbmetaKey>,

    /// Additional vbmeta keys for the `validate_public_key_for_partition()` callback.
    ///
    /// Stored as a map of {partition_name: (key, rollback_location)}. Querying keys for partitions
    /// not in this map will return `IoError::Io`.
    pub vbmeta_keys_for_partition: HashMap<&'static str, (FakeVbmetaKey, u32)>,

    /// Rollback indices. Accessing unknown locations will return `IoError::Io`.
    pub rollbacks: HashMap<usize, u64>,

    /// Unlock state. Set an error to simulate IoError during access.
    pub unlock_state: IoResult<bool>,

    /// Persistent named values. Set an error to simulate `IoError` during access. Writing
    /// a non-existent persistent value will create it; to simulate `NoSuchValue` instead,
    /// create an entry with `Err(IoError::NoSuchValue)` as the value.
    pub persistent_values: HashMap<String, IoResult<Vec<u8>>>,

    /// Set to true to enable `CertOps`; defaults to false.
    pub use_cert: bool,

    /// Cert permanent attributes, or `None` to trigger `IoError` on access.
    pub cert_permanent_attributes: Option<CertPermanentAttributes>,

    /// Cert permament attributes hash, or `None` to trigger `IoError` on access.
    pub cert_permanent_attributes_hash: Option<[u8; SHA256_DIGEST_SIZE]>,

    /// Cert key versions; will be updated by the `set_key_version()` cert callback.
    pub cert_key_versions: HashMap<usize, u64>,

    /// Fake RNG values to provide, or `IoError` if there aren't enough.
    pub cert_fake_rng: Vec<u8>,
}

impl<'a> TestOps<'a> {
    /// Adds a fake on-disk partition with the given contents.
    ///
    /// Reduces boilerplate a bit by taking in a raw array and returning a &mut so tests can
    /// do something like this:
    ///
    /// ```
    /// test_ops.add_partition("foo", [1, 2, 3, 4]);
    /// test_ops.add_partition("bar", [0, 0]).uuid = uuid!(...);
    /// ```
    pub fn add_partition<T: Into<Vec<u8>>>(
        &mut self,
        name: &'static str,
        contents: T,
    ) -> &mut FakePartition<'a> {
        self.partitions.insert(
            name,
            FakePartition::new(PartitionContents::FromDisk(contents.into())),
        );
        self.partitions.get_mut(name).unwrap()
    }

    /// Adds a preloaded partition with the given contents.
    ///
    /// Same a `add_partition()` except that the preloaded data is not owned by
    /// the `TestOps` but passed in, which means it can outlive `TestOps`.
    pub fn add_preloaded_partition(
        &mut self,
        name: &'static str,
        contents: &'a [u8],
    ) -> &mut FakePartition<'a> {
        self.partitions.insert(
            name,
            FakePartition::new(PartitionContents::Preloaded(contents)),
        );
        self.partitions.get_mut(name).unwrap()
    }

    /// Adds a persistent value with the given state.
    ///
    /// Reduces boilerplate by allowing array input:
    ///
    /// ```
    /// test_ops.add_persistent_value("foo", Ok(b"contents"));
    /// test_ops.add_persistent_value("bar", Err(IoError::NoSuchValue));
    /// ```
    pub fn add_persistent_value(&mut self, name: &str, contents: IoResult<&[u8]>) {
        self.persistent_values
            .insert(name.into(), contents.map(|b| b.into()));
    }

    /// Internal helper to validate a vbmeta key.
    fn validate_fake_key(
        &mut self,
        partition: Option<&str>,
        public_key: &[u8],
        public_key_metadata: Option<&[u8]>,
    ) -> IoResult<bool> {
        let fake_key = match partition {
            None => self.default_vbmeta_key.as_ref(),
            Some(p) => self.vbmeta_keys_for_partition.get(p).map(|(key, _)| key),
        }
        .ok_or(IoError::Io)?;

        match fake_key {
            FakeVbmetaKey::Avb {
                public_key: expected_key,
                public_key_metadata: expected_metadata,
            } => {
                // avb: only accept if it matches the hardcoded key + metadata.
                Ok(expected_key == public_key
                    && expected_metadata.as_deref() == public_key_metadata)
            }
            FakeVbmetaKey::Cert => {
                // avb_cert: forward to the cert helper function.
                cert_validate_vbmeta_public_key(self, public_key, public_key_metadata)
            }
        }
    }
}

impl Default for TestOps<'_> {
    fn default() -> Self {
        Self {
            partitions: HashMap::new(),
            default_vbmeta_key: None,
            vbmeta_keys_for_partition: HashMap::new(),
            rollbacks: HashMap::new(),
            unlock_state: Err(IoError::Io),
            persistent_values: HashMap::new(),
            use_cert: false,
            cert_permanent_attributes: None,
            cert_permanent_attributes_hash: None,
            cert_key_versions: HashMap::new(),
            cert_fake_rng: Vec::new(),
        }
    }
}

impl<'a> Ops<'a> for TestOps<'a> {
    fn read_from_partition(
        &mut self,
        partition: &CStr,
        offset: i64,
        buffer: &mut [u8],
    ) -> IoResult<usize> {
        let contents = self
            .partitions
            .get(partition.to_str()?)
            .ok_or(IoError::NoSuchPartition)?
            .contents
            .as_slice();

        // Negative offset means count backwards from the end.
        let offset = {
            if offset < 0 {
                offset
                    .checked_add(i64::try_from(contents.len()).unwrap())
                    .unwrap()
            } else {
                offset
            }
        };
        if offset < 0 {
            return Err(IoError::RangeOutsidePartition);
        }
        let offset = usize::try_from(offset).unwrap();

        if offset >= contents.len() {
            return Err(IoError::RangeOutsidePartition);
        }

        // Truncating is allowed for reads past the partition end.
        let end = min(offset.checked_add(buffer.len()).unwrap(), contents.len());
        let bytes_read = end - offset;

        buffer[..bytes_read].copy_from_slice(&contents[offset..end]);
        Ok(bytes_read)
    }

    fn get_preloaded_partition(&mut self, partition: &CStr) -> IoResult<&'a [u8]> {
        match self.partitions.get(partition.to_str()?) {
            Some(FakePartition {
                contents: PartitionContents::Preloaded(preloaded),
                ..
            }) => Ok(&preloaded[..]),
            _ => Err(IoError::NotImplemented),
        }
    }

    fn validate_vbmeta_public_key(
        &mut self,
        public_key: &[u8],
        public_key_metadata: Option<&[u8]>,
    ) -> IoResult<bool> {
        self.validate_fake_key(None, public_key, public_key_metadata)
    }

    fn read_rollback_index(&mut self, location: usize) -> IoResult<u64> {
        self.rollbacks.get(&location).ok_or(IoError::Io).copied()
    }

    fn write_rollback_index(&mut self, location: usize, index: u64) -> IoResult<()> {
        *(self.rollbacks.get_mut(&location).ok_or(IoError::Io)?) = index;
        Ok(())
    }

    fn read_is_device_unlocked(&mut self) -> IoResult<bool> {
        self.unlock_state.clone()
    }

    #[cfg(feature = "uuid")]
    fn get_unique_guid_for_partition(&mut self, partition: &CStr) -> IoResult<Uuid> {
        self.partitions
            .get(partition.to_str()?)
            .map(|p| p.uuid)
            .ok_or(IoError::NoSuchPartition)
    }

    fn get_size_of_partition(&mut self, partition: &CStr) -> IoResult<u64> {
        self.partitions
            .get(partition.to_str()?)
            .map(|p| u64::try_from(p.contents.as_slice().len()).unwrap())
            .ok_or(IoError::NoSuchPartition)
    }

    fn read_persistent_value(&mut self, name: &CStr, value: &mut [u8]) -> IoResult<usize> {
        match self
            .persistent_values
            .get(name.to_str()?)
            .ok_or(IoError::NoSuchValue)?
        {
            // If we were given enough space, write the value contents.
            Ok(contents) if contents.len() <= value.len() => {
                value[..contents.len()].clone_from_slice(contents);
                Ok(contents.len())
            }
            // Not enough space, tell the caller how much we need.
            Ok(contents) => Err(IoError::InsufficientSpace(contents.len())),
            // Simulated error, return it.
            Err(e) => Err(e.clone()),
        }
    }

    fn write_persistent_value(&mut self, name: &CStr, value: &[u8]) -> IoResult<()> {
        let name = name.to_str()?;

        // If the test requested a simulated error on this value, return it.
        if let Some(Err(e)) = self.persistent_values.get(name) {
            return Err(e.clone());
        }

        self.persistent_values
            .insert(name.to_string(), Ok(value.to_vec()));
        Ok(())
    }

    fn erase_persistent_value(&mut self, name: &CStr) -> IoResult<()> {
        let name = name.to_str()?;

        // If the test requested a simulated error on this value, return it.
        if let Some(Err(e)) = self.persistent_values.get(name) {
            return Err(e.clone());
        }

        self.persistent_values.remove(name);
        Ok(())
    }

    fn validate_public_key_for_partition(
        &mut self,
        partition: &CStr,
        public_key: &[u8],
        public_key_metadata: Option<&[u8]>,
    ) -> IoResult<PublicKeyForPartitionInfo> {
        let partition = partition.to_str()?;

        let rollback_index_location = self
            .vbmeta_keys_for_partition
            .get(partition)
            .ok_or(IoError::Io)?
            .1;

        Ok(PublicKeyForPartitionInfo {
            trusted: self.validate_fake_key(Some(partition), public_key, public_key_metadata)?,
            rollback_index_location,
        })
    }

    fn cert_ops(&mut self) -> Option<&mut dyn CertOps> {
        match self.use_cert {
            true => Some(self),
            false => None,
        }
    }
}

impl<'a> CertOps for TestOps<'a> {
    fn read_permanent_attributes(
        &mut self,
        attributes: &mut CertPermanentAttributes,
    ) -> IoResult<()> {
        *attributes = self.cert_permanent_attributes.ok_or(IoError::Io)?;
        Ok(())
    }

    fn read_permanent_attributes_hash(&mut self) -> IoResult<[u8; SHA256_DIGEST_SIZE]> {
        self.cert_permanent_attributes_hash.ok_or(IoError::Io)
    }

    fn set_key_version(&mut self, rollback_index_location: usize, key_version: u64) {
        self.cert_key_versions
            .insert(rollback_index_location, key_version);
    }

    fn get_random(&mut self, bytes: &mut [u8]) -> IoResult<()> {
        if bytes.len() > self.cert_fake_rng.len() {
            return Err(IoError::Io);
        }

        let leftover = self.cert_fake_rng.split_off(bytes.len());
        bytes.copy_from_slice(&self.cert_fake_rng[..]);
        self.cert_fake_rng = leftover;
        Ok(())
    }
}
