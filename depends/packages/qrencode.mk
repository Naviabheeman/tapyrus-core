package=qrencode
$(package)_version=4.1.1
$(package)_download_path=https://fukuchi.org/works/qrencode/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=e455d9732f8041cf5b9c388e345a641fd15707860f928e94507b1961256a6923

define $(package)_set_vars
$(package)_config_opts=--disable-shared --without-tools --without-tests --without-png
$(package)_config_opts += --disable-gprof --disable-gcov --disable-mudflap
$(package)_config_opts += --disable-dependency-tracking --enable-option-checking
$(package)_cflags += -Wno-int-conversion -Wno-implicit-function-declaration
$(package)_cmake_opts := -DWITH_TOOLS=NO -DWITH_TESTS=NO -DGPROF=OFF -DCOVERAGE=OFF
$(package)_cmake_opts += -DCMAKE_DISABLE_FIND_PACKAGE_PNG=TRUE -DWITHOUT_PNG=ON
$(package)_cmake_opts += -DCMAKE_DISABLE_FIND_PACKAGE_ICONV=TRUE
endef

define $(package)_preprocess_cmds
  cp -f $(BASEDIR)/config.guess $(BASEDIR)/config.sub use
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
  if [ -f lib/*.la ]; then rm lib/*.la; fi
endef
