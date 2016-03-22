# included mk file for the sprout SIP router

ifndef PJSIP_DIR
  include ${MK_DIR}/pjsip.mk
endif

ifndef LIBMEM_DIR
  include ${MK_DIR}/libmemcached.mk
endif

SPROUT_DIR := ${ROOT}/sprout
SPROUT_TEST_DIR := ${ROOT}/tests

sprout: pjsip libmemcached
	${MAKE} -C ${SPROUT_DIR}

sprout_test:
	${MAKE} -C ${SPROUT_DIR} test

sprout_runtest:
	${MAKE} -C ${SPROUT_DIR} run_test

sprout_clean:
	${MAKE} -C ${SPROUT_DIR} clean
	-${MAKE} -C ${SPROUT_TEST_DIR} clean

sprout_distclean: sprout_clean

.PHONY: sprout sprout_test sprout_clean sprout_distclean
