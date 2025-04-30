package=libevent
$(package)_version=2.1.12-stable
$(package)_download_path=https://github.com/libevent/libevent/releases/download/release-$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=92e6de1be9ec176428fd2367677e61ceffc2ee1cb119035037a27d346b0403bb

# When building for Windows, we set _WIN32_WINNT to target the same Windows
# version as we do in configure. Due to quirks in libevents build system, this
# is also required to enable support for ipv6. See #19375.
define $(package)_set_vars
  $(package)_config_opts=--disable-shared --disable-openssl --disable-libevent-regress --disable-samples
  $(package)_config_opts += --disable-dependency-tracking --enable-option-checking
  $(package)_config_opts_release=--disable-debug-mode
  $(package)_cppflags_mingw32=-D_WIN32_WINNT=0x0601

  # CMake configuration options
  $(package)_cmake_opts=-DCMAKE_BUILD_TYPE=None -DEVENT__DISABLE_BENCHMARK=ON -DEVENT__DISABLE_OPENSSL=ON
  $(package)_cmake_opts+=-DEVENT__DISABLE_SAMPLES=ON -DEVENT__DISABLE_REGRESS=ON
  $(package)_cmake_opts+=-DEVENT__DISABLE_TESTS=ON -DEVENT__LIBRARY_TYPE=STATIC
  $(package)_cflags += -fdebug-prefix-map=$($(package)_extract_dir)=/usr -fmacro-prefix-map=$($(package)_extract_dir)=/usr
  $(package)_cppflags += -D_GNU_SOURCE -D_FORTIFY_SOURCE=3
endef

define $(package)_preprocess_cmds
  cp -f $(BASEDIR)/config.guess $(BASEDIR)/config.sub build-aux
endef

define $(package)_config_cmds
  $($(package)_autoconf)
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
  if [ -f lib/*.la ]; then  rm lib/*.la; fi && \
  if [ -f include/ev*.h ]; then rm include/ev*.h; fi && \
  if [ -d include/event2 ] && [ -f include/event2/*_compat.h ]; then rm include/event2/*_compat.h; fi
endef
