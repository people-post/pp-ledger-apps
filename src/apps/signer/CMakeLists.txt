# pp-admin-signer: review, sign and merge operations, offline (AD005: it links no network code).
add_executable(pp-admin-signer main.cpp)
target_link_libraries(pp-admin-signer PRIVATE admin_ui)
pp_admin_apply_warnings(pp-admin-signer)
