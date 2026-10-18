# First-party targets build with warnings, as errors by default.
option(PP_APPS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

function(pp_apps_apply_warnings target)
  if(MSVC)
    # /W3 for now: the Windows build is new (the wallet's, WL008); raise to /W4 /WX once it is clean.
    target_compile_options(${target} PRIVATE /W3 /permissive- /utf-8)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    return()
  endif()
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter)
  if(PP_APPS_WARNINGS_AS_ERRORS)
    target_compile_options(${target} PRIVATE -Werror)
  endif()
endfunction()
