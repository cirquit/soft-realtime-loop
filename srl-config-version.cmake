

 set(PACKAGE_VERSION @srl_VERSION@)
 if("${PACKAGE_FIND_VERSION_MAJOR}" EQUAL "@srl_VERSION_MAJOR@")
     if ("${PACKAGE_FIND_VERSION_MINOR}" EQUAL "@srl_VERSION_MINOR@")
         set(PACKAGE_VERSION_EXACT TRUE)
     elseif("${PACKAGE_FIND_VERSION_MINOR}" LESS "@srl_VERSION_MINOR@")
         set(PACKAGE_VERSION_COMPATIBLE TRUE)
     else()
         set(PACKAGE_VERSION_UNSUITABLE TRUE)
     endif()
 else()
     set(PACKAGE_VERSION_UNSUITABLE TRUE)
 endif()