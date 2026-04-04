from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from conan.tools.files import collect_libs, rmdir


class MatchmakingProxy(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    python_requires = "shared/1.0.0"


    options = {
        "with_ssl_verification": [True, False],
    }
    default_options = {"with_ssl_verification": True}

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = False  # workaround because this leads to useless options in cmake-tools configure
        tc.variables["WITH_SSL_VERIFICATION"] = self.options.with_ssl_verification
        tc.generate()

    def configure(self):
        # We can control the options of our dependencies based on current options
        self.options["catch2"].with_main = True
        self.options["catch2"].with_benchmark = True
        self.options["my_web_socket"].log_co_spawn_print_exception = True
        self.options["my_web_socket"].log_write = True
        self.options["my_web_socket"].log_read = True
        self.options["my_web_socket"].log_boost_asio = False

    def requirements(self):
        sharedConan = self.python_requires["shared"].module.SharedConan
        all_deps = {**sharedConan.COMMON, **sharedConan.BACKEND}
        deps_to_use = [
            "boost",
            "confu_soci",
            "magic_enum",
            "libsodium",
            "confu_json",
            "confu_algorithm",
            "sml",
            "login_matchmaking_game_shared",
            "my_web_socket",
            "sqlite3",
            "openssl",
            "certify",
        ]
        for pkg_name in deps_to_use:
            version, isModernDurak = all_deps[pkg_name]
            if pkg_name == "certify":
                if self.options.with_ssl_verification:
                    self.requires(
                        f"{pkg_name}/{version}{'@modern-durak' if isModernDurak else ''}"
                    )
            else:
                self.requires(
                    f"{pkg_name}/{version}{'@modern-durak' if isModernDurak else ''}"
                )


        ### only for testing please do not put this in the package build recept ###
        self.requires("modern_durak_game_shared/latest@modern-durak")
        self.requires("modern_durak_game_option/latest@modern-durak")
        self.requires("catch2/2.13.9")
        ###########################################################################
