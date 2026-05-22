# Third-Party Licenses

This file inventories the third-party software used by ysm-ng. Standard
license names use SPDX identifiers. `AND` denotes combined terms and `OR`
denotes a choice offered by the upstream project.

> **Important:** This is an informational index. It does not reproduce every
> license, notice, attribution, or patent text and is not, by itself, a
> complete compliance bundle for source or binary distribution. Release
> artifacts must be reviewed separately and accompanied by all texts and
> attributions required by the dependencies they contain.

The Conan inventory is based on [`conanfile.py`](conanfile.py) and the pinned
sources in `third-party/*/conandata.yml`.

## Direct dependencies

| Component | Conan reference | Pinned source | License | Notes |
| --- | --- | --- | --- | --- |
| Abseil | `abseil/20260107.1@ysm/stable` | [255c84d](https://github.com/abseil/abseil-cpp/tree/255c84dadd029fd8ad25c5efb5933e47beaa00c7) | [Apache-2.0](https://github.com/abseil/abseil-cpp/blob/255c84dadd029fd8ad25c5efb5933e47beaa00c7/LICENSE) |  |
| BLAKE3 | `blake3/1.8.5@ysm/stable` | [93a431c](https://github.com/BLAKE3-team/BLAKE3/tree/93a431c78a52d7ccf0f366f106467f5070e6075e) | [CC0-1.0](https://github.com/BLAKE3-team/BLAKE3/blob/93a431c78a52d7ccf0f366f106467f5070e6075e/LICENSE_CC0) OR [Apache-2.0](https://github.com/BLAKE3-team/BLAKE3/blob/93a431c78a52d7ccf0f366f106467f5070e6075e/LICENSE_A2) OR [Apache-2.0 WITH LLVM-exception](https://github.com/BLAKE3-team/BLAKE3/blob/93a431c78a52d7ccf0f366f106467f5070e6075e/LICENSE_A2LLVM) |  |
| cglm | `cglm/0.9.6@ysm/stable` | [144d1e7](https://github.com/recp/cglm/tree/144d1e7c29b3b0c6dede7917a0476cc95248559c) | [MIT](https://github.com/recp/cglm/blob/144d1e7c29b3b0c6dede7917a0476cc95248559c/LICENSE) |  |
| cpu_features | `cpu_features/0.11.0@ysm/stable` | [81d13c4](https://github.com/google/cpu_features/tree/81d13c49649f0714dd41fb56bb246398b6584085) | [Apache-2.0](https://github.com/google/cpu_features/blob/81d13c49649f0714dd41fb56bb246398b6584085/LICENSE) |  |
| Crypto++ | `cryptopp/8.9.0@ysm/stable` | [843d74c](https://github.com/weidai11/cryptopp/tree/843d74c7c97f9e19a615b8ff3c0ca06599ca501b) | [BSL-1.0 AND BSD-3-Clause](https://github.com/weidai11/cryptopp/blob/843d74c7c97f9e19a615b8ff3c0ca06599ca501b/License.txt) | Includes public-domain portions. The [cryptopp-cmake wrapper at f815f62](https://github.com/abdes/cryptopp-cmake/tree/f815f6284684be6ab03af4b6c273359331c61241) is [BSD-3-Clause](https://github.com/abdes/cryptopp-cmake/blob/f815f6284684be6ab03af4b6c273359331c61241/LICENSE). |
| FlatBuffers | `flatbuffers/25.12.19-2026-02-06@ysm/stable` | [03fffb2](https://github.com/google/flatbuffers/tree/03fffb25e2d777462b719cb4964249c30b19d58f) | [Apache-2.0](https://github.com/google/flatbuffers/blob/03fffb25e2d777462b719cb4964249c30b19d58f/LICENSE) |  |
| JNI headers | `jni/8.0.0@ysm/stable` | [local snapshot](third-party/jni/src) | [GPL-2.0-only WITH Classpath-exception-2.0](third-party/jni/src/jni.h) | `8.0.0` is a local recipe label; the exact upstream revision is not recorded. The headers contain license notices, but the standalone GPL and Classpath Exception texts are not stored in this repository. |
| libavif | `avif/1.4.1@ysm/stable` | [6543b22](https://github.com/AOMediaCodec/libavif/tree/6543b22b5bc706c53f038a16fe515f921556d9b3) | [BSD-2-Clause](https://github.com/AOMediaCodec/libavif/blob/6543b22b5bc706c53f038a16fe515f921556d9b3/LICENSE) |  |
| libjpeg-turbo | `jpeg-turbo/3.1.90@ysm/stable` | [e1dbfa7](https://github.com/libjpeg-turbo/libjpeg-turbo/tree/e1dbfa7be7b7e54922020051dc77781e92739700) | [IJG AND BSD-3-Clause](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/e1dbfa7be7b7e54922020051dc77781e92739700/LICENSE.md) | See the upstream [IJG notice](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/e1dbfa7be7b7e54922020051dc77781e92739700/README.ijg); its binary attribution requirements apply where relevant. |
| libspng | `spng/0.7.4@ysm/stable` | [fb76800](https://github.com/randy408/libspng/tree/fb768002d4288590083a476af628e51c3f1d47cd) | [BSD-2-Clause](https://github.com/randy408/libspng/blob/fb768002d4288590083a476af628e51c3f1d47cd/LICENSE) |  |
| libwebp | `webp/1.6.0@ysm/stable` | [4fa2191](https://github.com/webmproject/libwebp/tree/4fa21912338357f89e4fd51cf2368325b59e9bd9) | [BSD-3-Clause](https://github.com/webmproject/libwebp/blob/4fa21912338357f89e4fd51cf2368325b59e9bd9/COPYING) | See upstream [PATENTS](https://github.com/webmproject/libwebp/blob/4fa21912338357f89e4fd51cf2368325b59e9bd9/PATENTS). |
| libyuv | `yuv/0.0.1892@ysm/stable` | [9d98aae](https://github.com/lemenkov/libyuv/tree/9d98aaefe7a5e2710aa6175d44da38892400b381) | [BSD-3-Clause](https://github.com/lemenkov/libyuv/blob/9d98aaefe7a5e2710aa6175d44da38892400b381/LICENSE) | See upstream [PATENTS](https://github.com/lemenkov/libyuv/blob/9d98aaefe7a5e2710aa6175d44da38892400b381/PATENTS). |
| LZMA SDK | `lzma/25.01@ysm/stable` | [18dab99](https://github.com/sisong/lzma/tree/18dab99e5796bd27d1feabcec49708563df7cb98) | [LicenseRef-LZMA-SDK-Public-Domain](https://github.com/sisong/lzma/blob/18dab99e5796bd27d1feabcec49708563df7cb98/DOC/lzma-sdk.txt) | Upstream places the LZMA SDK in the public domain; no standard SPDX identifier precisely represents that statement. |
| magic_enum | `magic_enum/0.9.8@ysm/stable` | [1384769](https://github.com/Neargye/magic_enum/tree/1384769c66bd16ec9bb1353f45fe8ec8ccc12dbd) | [MIT](https://github.com/Neargye/magic_enum/blob/1384769c66bd16ec9bb1353f45fe8ec8ccc12dbd/LICENSE) |  |
| mimalloc | `mimalloc/3.3.2@ysm/stable` | [30b2d9d](https://github.com/microsoft/mimalloc/tree/30b2d9d89099bee08e9f67a1ffb3e12e7ba45227) | [MIT](https://github.com/microsoft/mimalloc/blob/30b2d9d89099bee08e9f67a1ffb3e12e7ba45227/LICENSE) |  |
| minizip-ng | `minizip-ng/4.2.1@ysm/stable` | [26b4619](https://github.com/zlib-ng/minizip-ng/tree/26b4619120f714cb76c0c52cdd48223bee944d73) | [Zlib](https://github.com/zlib-ng/minizip-ng/blob/26b4619120f714cb76c0c52cdd48223bee944d73/LICENSE) |  |
| optional_ref | `optional_ref/0.3.1@ysm/stable` | [252f1fe](https://github.com/gharveymn/optional_ref/tree/252f1fe0e30d2654dbc3c60407e16f25b8a36a4c) | [MIT](https://github.com/gharveymn/optional_ref/blob/252f1fe0e30d2654dbc3c60407e16f25b8a36a4c/docs/LICENSE) |  |
| Opus | `opus/1.6.1@ysm/stable` | [22244de](https://github.com/xiph/opus/tree/22244de5a79bd1d6d623c32e72bf1954b56235be) | [BSD-3-Clause](https://github.com/xiph/opus/blob/22244de5a79bd1d6d623c32e72bf1954b56235be/COPYING) |  |
| Proxy | `proxy/4.0.2@ysm/stable` | [bcbe0c7](https://github.com/ngcpp/proxy/tree/bcbe0c792cd42cb48edfbbdb1e272255cbe73cae) | [MIT](https://github.com/ngcpp/proxy/blob/bcbe0c792cd42cb48edfbbdb1e272255cbe73cae/LICENSE) |  |
| pystring | `pystring/1.1.5@ysm/stable` | [381829c](https://github.com/imageworks/pystring/tree/381829c2c1696ffec9277b339952a9588e6e67cf) | [BSD-3-Clause](https://github.com/imageworks/pystring/blob/381829c2c1696ffec9277b339952a9588e6e67cf/LICENSE) |  |
| yalantinglibs | `yalantinglibs/0.6.1@ysm/stable` | [8c1f2ea](https://github.com/alibaba/yalantinglibs/tree/8c1f2ea927a5a879046067a686599f825a370858) | [Apache-2.0](https://github.com/alibaba/yalantinglibs/blob/8c1f2ea927a5a879046067a686599f825a370858/LICENSE) | Upstream also provides a [NOTICE](https://github.com/alibaba/yalantinglibs/blob/8c1f2ea927a5a879046067a686599f825a370858/NOTICE). |
| zlib-ng | `zlib-ng/2.3.3@ysm/stable` | [1273109](https://github.com/zlib-ng/zlib-ng/tree/12731092979c6d07f42da27da673a9f6c7b13586) | [Zlib](https://github.com/zlib-ng/zlib-ng/blob/12731092979c6d07f42da27da673a9f6c7b13586/LICENSE.md) |  |
| Zstandard | `zstd/1.5.7@ysm/stable` | [f8745da](https://github.com/facebook/zstd/tree/f8745da6ff1ad1e7bab384bd1f9d742439278e99) | [BSD-3-Clause](https://github.com/facebook/zstd/blob/f8745da6ff1ad1e7bab384bd1f9d742439278e99/LICENSE) OR [GPL-2.0-only](https://github.com/facebook/zstd/blob/f8745da6ff1ad1e7bab384bd1f9d742439278e99/COPYING) |  |

## Transitive runtime dependencies

| Component | Conan reference | Pinned source | License | Pulled in by |
| --- | --- | --- | --- | --- |
| dav1d | `dav1d/1.5.3@ysm/stable` | [b546257](https://github.com/videolan/dav1d/tree/b546257f770768b2c88258c533da38b91a06f737) | [BSD-2-Clause](https://github.com/videolan/dav1d/blob/b546257f770768b2c88258c533da38b91a06f737/COPYING) | libavif; see upstream [PATENTS](https://github.com/videolan/dav1d/blob/b546257f770768b2c88258c533da38b91a06f737/doc/PATENTS). |
| SVT-AV1 | `svt-av1/4.1.0@ysm/stable` | [c04f951](https://gitlab.com/AOMediaCodec/SVT-AV1/-/tree/c04f951541ad600e0d9c10836f2ab7b9bc69816d) | [BSD-3-Clause-Clear](https://gitlab.com/AOMediaCodec/SVT-AV1/-/blob/c04f951541ad600e0d9c10836f2ab7b9bc69816d/LICENSE.md) | libavif, except on Android; see upstream [PATENTS](https://gitlab.com/AOMediaCodec/SVT-AV1/-/blob/c04f951541ad600e0d9c10836f2ab7b9bc69816d/PATENTS.md). |

## Test-only dependencies

| Component | Conan reference | Pinned source | License | Relationship |
| --- | --- | --- | --- | --- |
| Google Benchmark | `benchmark/1.9.5@ysm/stable` | [192ef10](https://github.com/google/benchmark/tree/192ef10025eb2c4cdd392bc502f0c852196baa48) | [Apache-2.0](https://github.com/google/benchmark/blob/192ef10025eb2c4cdd392bc502f0c852196baa48/LICENSE) | Direct test requirement. |
| GoogleTest | `googletest/1.17.0@ysm/stable` | [52eb810](https://github.com/google/googletest/tree/52eb8108c5bdec04579160ae17225d66034bd723) | [BSD-3-Clause](https://github.com/google/googletest/blob/52eb8108c5bdec04579160ae17225d66034bd723/LICENSE) | Direct test requirement. |
| RE2 | `re2/2025-11-05@ysm/stable` | [927f5d5](https://github.com/google/re2/tree/927f5d53caf8111721e734cf24724686bb745f55) | [BSD-3-Clause](https://github.com/google/re2/blob/927f5d53caf8111721e734cf24724686bb745f55/LICENSE) | Transitive requirement of GoogleTest. |

## Third-party code stored in this repository

| Component | Local file | Upstream reference | License | Notes |
| --- | --- | --- | --- | --- |
| STX `CStringView` | [`modules/core/src/c_string_view.h`](modules/core/src/c_string_view.h) | [STX file at 79b4ce0](https://github.com/lamarrr/STX/blob/79b4ce0c2565e0fe70c9953e8d6cefbde8f0a0ab/include/stx/c_string_view.h) | [MIT](https://github.com/lamarrr/STX/blob/79b4ce0c2565e0fe70c9953e8d6cefbde8f0a0ab/LICENSE) | The local file identifies STX as its source; the exact imported revision is not recorded. |
| MiniOgg | [`modules/core/src/codec/mini_ogg.h`](modules/core/src/codec/mini_ogg.h) | Provenance not recorded | [0BSD](modules/core/src/codec/mini_ogg.h) | Copyright 2023 John Regan; the complete license text is embedded in the local header. |

## Exclusions

This inventory excludes build tools, compilers, C and C++ runtimes, Iconv,
Windows synchronization libraries, operating-system frameworks, and other
system-provided components. Their terms are determined by the build and target
environments rather than the dependency recipes in this repository.

## Maintenance

Update this file in the same change whenever a dependency reference, pinned
source revision, license, notice, patent file, or stored third-party source is
added, removed, or changed. Prefer immutable commit links, and do not infer an
upstream revision when the repository does not record one.
