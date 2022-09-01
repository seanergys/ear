AC_DEFUN([AX_SET_ARCHITECTURES],
[
	AC_ARG_ENABLE([arm64],
		AS_HELP_STRING([--arch-arm64], [Compiles for ARM64 architecture])
        )

    if test "x$enable_arm64" = "xno"; then
        ARCH_X86=1
        ARCH_ARM=0
    else
        ARCH_X86=0
        ARCH_ARM=1
    fi

    AC_SUBST(ARCH_X86)
    AC_SUBST(ARCH_ARM)
])
