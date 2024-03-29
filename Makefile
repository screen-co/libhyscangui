
ifeq ($(basename $(notdir $(MAKE))), mingw32-make)
  CMAKE_GENERATOR = "MinGW Makefiles"
  MAKE_DIR = md
  REMOVE_DIR = rd /s /q
else
  CMAKE_GENERATOR = "Unix Makefiles"
  MAKE_DIR = mkdir -p
  REMOVE_DIR = rm -rf
endif

PREFIX ?= /usr

all: release

test: release-test

release:
	@-${MAKE_DIR} bin
	@-${MAKE_DIR} build
	@-${MAKE_DIR} doc/documentation
	@cd build && cmake -G $(CMAKE_GENERATOR) \
                       -D CMAKE_BUILD_TYPE=Release \
                       -D CMAKE_INSTALL_PREFIX=$(PREFIX) \
                       -D HYSCAN_USE_SYS_LIBS=$(USE_SYS_LIBS) \
                       ..
	@$(MAKE) -C build

release-test: release
	@$(MAKE) -C build test

debug:
	@-${MAKE_DIR} bin
	@-${MAKE_DIR} build
	@-${MAKE_DIR} doc/documentation
	@cd build && cmake -G $(CMAKE_GENERATOR) \
                       -D CMAKE_BUILD_TYPE=Debug \
                       -D CMAKE_INSTALL_PREFIX=$(PREFIX) \
                       -D HYSCAN_USE_SYS_LIBS=$(USE_SYS_LIBS) \
                       ..
	@$(MAKE) -C build

debug-test: debug
	@$(MAKE) -C build test

install: install-runtime

install-all: release
	@echo "Performing installation"
	@cd build && cmake -D CMAKE_INSTALL_DO_STRIP=YES \
                       -P cmake_install.cmake

install-runtime: release
	@echo "Performing runtime installation"
	@cd build && cmake -D COMPONENT=runtime \
                       -D CMAKE_INSTALL_DO_STRIP=YES \
                       -P cmake_install.cmake

install-development: release
	@echo "Performing development installation"
	@cd build && cmake -D COMPONENT=development \
                       -P cmake_install.cmake

install-test: install-runtime
	@echo "Performing test installation"
	@cd build && cmake -D COMPONENT=test \
                       -D CMAKE_INSTALL_DO_STRIP=YES \
                       -P cmake_install.cmake

doc:
	@cd doc && doxygen

clean:
	@echo "Cleaning build directory"
	-@$(MAKE) -C build clean

distclean: clean
	@echo "Removing build directory"
	-@${REMOVE_DIR} bin
	-@${REMOVE_DIR} build
	-@${REMOVE_DIR} doc/documentation

.PHONY: doc
