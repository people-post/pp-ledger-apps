# First-party targets build with warnings, as errors by default.
option(PP_ADMIN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

function(pp_admin_apply_warnings target)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter)
  if(PP_ADMIN_WARNINGS_AS_ERRORS)
    target_compile_options(${target} PRIVATE -Werror)
  endif()
endfunction()
