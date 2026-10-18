# apps_online: the screens of the apps that talk to a node — the node setting, forms that build
# operations from the chain — and submitting (Session::submit). The Signer links none of it.
add_library(apps_online STATIC
    AdminScreens.cpp
    AdminScreens.h
    GenesisScreens.cpp
    GenesisScreens.h
    NetworkScreen.cpp
    OperationForm.cpp
    OperationForm.h
    RegistrarScreen.cpp
    RegistrarScreen.h
    TokenScreens.cpp
    TokenScreens.h
    NetworkScreen.h
    Online.cpp
    Online.h
    TransferScreen.cpp
    TransferScreen.h
)
target_include_directories(apps_online PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(apps_online PUBLIC apps_ui apps_net)
pp_apps_apply_warnings(apps_online)
