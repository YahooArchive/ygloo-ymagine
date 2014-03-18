# Helper makefile to build iOS library for all architectures
YMAGINE_IOS_DIR:=$(shell pwd)
YMAGINE_DIR:=$(YMAGINE_IOS_DIR)/..
BUILDCONFIG_DIR:=$(YMAGINE_DIR)/../..

all: libs/libyahoo_ymagine.a

libs/libyahoo_ymagine.a: $(BUILDCONFIG_DIR)/out/target/ios/libyahoo_ymagine.a
	@mkdir -p libs
	cp -f $< $@


$(BUILDCONFIG_DIR)/out/target/ios/libyahoo_ymagine.a:
	make -j 4 -C $(BUILDCONFIG_DIR) all ios TARGET_ARCH=armv7
	make -j 4 -C $(BUILDCONFIG_DIR) all ios TARGET_ARCH=armv7s
	make -j 4 -C $(BUILDCONFIG_DIR) all ios TARGET_ARCH=arm64
	make -j 4 -C $(BUILDCONFIG_DIR) all ios TARGET_ARCH=i386
	make -j 4 -C $(BUILDCONFIG_DIR) all ios TARGET_ARCH=x86_64
	make -j 4 -C $(BUILDCONFIG_DIR) ios combo
