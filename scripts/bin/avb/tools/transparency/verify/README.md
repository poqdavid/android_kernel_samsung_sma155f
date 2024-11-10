# Verifier of Binary Transparency for Pixel Factory Images

This repository contains code to read the transparency log for [Pixel Factory Images Binary Transparency](https://developers.google.com/android/binary_transparency/pixel_overview). See the particular section for this tool [here](https://developers.google.com/android/binary_transparency/pixel_verification#verifying-image-inclusion-inclusion-proof).

## Files and Directories
* `cmd/verifier/`
  * Contains the binary to read the transparency log. It is embedded with the public key of the log to verify log identity.
* `internal/`
  * Internal libraries for the verifier binary.

## Build
This module requires Go 1.17. Install [here](https://go.dev/doc/install), and run `go build cmd/verifier/verifier.go`.

An executable named `verifier` should be produced upon successful build.

## Usage
The verifier uses the checkpoint and the log contents (found at the [tile directory](https://developers.google.com/android/binary_transparency/tile)) to check that your image payload is in the transparency log, i.e. that it is published by Google.

To run the verifier after you have built it in the previous section:
```
$ ./verifier --payload_path=${PAYLOAD_PATH}
```

### Input
The verifier takes a `payload_path` as input.

Each Pixel Factory image corresponds to a [payload](https://developers.google.com/android/binary_transparency/pixel_overview#log_content) stored in the transparency log, the format of which is:
```
<build_fingerprint>\n<vbmeta_digest>\n
```
See [here](https://developers.google.com/android/binary_transparency/pixel_verification#construct-the-payload-for-verification) for a few methods detailing how to extract this payload from an image.

### Output
The output of the command is written to stdout:
  * `OK` if the image is included in the log, i.e. that this [claim](https://developers.google.com/android/binary_transparency/pixel_overview#claimant_model) is true,
  * `FAILURE` otherwise.

