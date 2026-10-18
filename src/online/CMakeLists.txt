# admin_online: the screens of the apps that talk to a node — the node setting, forms that build
# operations from the chain — and submitting (Session::submit). The Signer links none of it.
add_library(admin_online STATIC
    AdminScreens.cpp
    AdminScreens.h
    GenesisScreens.cpp
    GenesisScreens.h
    NetworkScreen.cpp
    OperationForm.cpp
    OperationForm.h
    RegistrarScreen.cpp
    RegistrarScreen.h
    NetworkScreen.h
    Online.cpp
    Online.h
    TransferScreen.cpp
    TransferScreen.h
)
target_include_directories(admin_online PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(admin_online PUBLIC admin_ui admin_net)
pp_admin_apply_warnings(admin_online)
