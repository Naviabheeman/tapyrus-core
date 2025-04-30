package=zeromq
$(package)_version=4.3.5
$(package)_download_path=https://github.com/zeromq/libzmq/releases/download/v$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=6653ef5910f17954861fe72332e68b03ca6e4d9c7160eb3a8de5a5a913bfab43
$(package)_patches = remove_libstd_link.patch
$(package)_patches += macos_mktemp_check.patch
$(package)_patches += builtin_sha1.patch
$(package)_patches += fix_have_windows.patch
$(package)_patches += openbsd_kqueue_headers.patch
$(package)_patches += cmake_minimum.patch
$(package)_patches += cacheline_undefined.patch
$(package)_patches += no_librt.patch

define $(package)_set_vars
  $(package)_config_opts = --without-docs --disable-shared --disable-valgrind
  $(package)_config_opts += --disable-perf --disable-curve-keygen --disable-curve --disable-libbsd
  $(package)_config_opts += --without-libsodium --without-libgssapi_krb5 --without-pgm --without-norm --without-vmci
  $(package)_config_opts += --disable-libunwind --disable-radix-tree --without-gcov --disable-dependency-tracking
  $(package)_config_opts += --disable-Werror --disable-drafts --enable-option-checking
  $(package)_cxxflags=-std=c++17
  $(package)_cmake_opts := -DCMAKE_BUILD_TYPE=None -DWITH_DOCS=OFF -DWITH_LIBSODIUM=OFF
  $(package)_cmake_opts += -DWITH_LIBBSD=OFF -DENABLE_CURVE=OFF -DENABLE_CPACK=OFF
  $(package)_cmake_opts += -DBUILD_SHARED=OFF -DBUILD_TESTS=OFF -DZMQ_BUILD_TESTS=OFF
  $(package)_cmake_opts += -DENABLE_DRAFTS=OFF -DZMQ_BUILD_TESTS=OFF
  $(package)_cxxflags += -fdebug-prefix-map=$($(package)_extract_dir)=/usr -fmacro-prefix-map=$($(package)_extract_dir)=/usr
  $(package)_config_opts_mingw32 += -DZMQ_WIN32_WINNT=0x0A00 -DZMQ_HAVE_IPC=OFF
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/remove_libstd_link.patch && \
  patch -p1 < $($(package)_patch_dir)/macos_mktemp_check.patch && \
  patch -p1 < $($(package)_patch_dir)/builtin_sha1.patch && \
  patch -p1 < $($(package)_patch_dir)/cacheline_undefined.patch && \
  patch -p1 < $($(package)_patch_dir)/fix_have_windows.patch && \
  patch -p1 < $($(package)_patch_dir)/openbsd_kqueue_headers.patch && \
  patch -p1 < $($(package)_patch_dir)/cmake_minimum.patch && \
  patch -p1 < $($(package)_patch_dir)/no_librt.patch
endef

define $(package)_config_cmds
  $($(package)_config_env) ./configure $($(package)_config_opts)
endef

define $(package)_cmake_config_cmds
  mkdir -p cmake_build && \
  $($(package)_cmake) -S $($(package)_extract_dir) -B cmake_build $($(package)_cmake_opts)
endef

define $(package)_build_cmds
  $(MAKE) -C cmake_build
endef

define $(package)_stage_cmds
  $(MAKE) -C cmake_build DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf bin share lib/*.la
endef
