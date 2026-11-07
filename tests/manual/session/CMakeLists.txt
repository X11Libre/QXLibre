# Session management tests. Not ctests: run the scripts under xvfb-run.
#   run.sh <xcb|xlibre> <work dir> <smtest> <smreq>         against xsm
#   global.sh <xcb|xlibre> <work dir> <smtest> <smsrv>      against smsrv, with SMTEST_CANCEL=1 to cancel
#   dbus-run-session -- ks.sh <xcb|xlibre> <work dir> <smtest>    against a private ksmserver
add_executable(smtest main.cpp)
target_link_libraries(smtest PRIVATE Qt::Gui)

add_executable(smreq smreq.c)
target_link_libraries(smreq PRIVATE X11::SM X11::ICE)

add_executable(smsrv smsrv.c)
target_link_libraries(smsrv PRIVATE X11::SM X11::ICE)
