
#
#
# suricata library.
#
#

set (SUBDIR ./src)
SET (LIBNAME suricata)

aux_source_directory (${SUBDIR} DIR_LIB_SRCS)
add_library (${LIBNAME} STATIC ${DIR_LIB_SRCS})

install (TARGETS ${LIBNAME} DESTINATION lib)
install (FILES ${HEADERS} DESTINATION include)

unset (SUBDIR)
unset (LIBNAME)
unset (HEADERS)

#
# < add your own library path here. >
#




