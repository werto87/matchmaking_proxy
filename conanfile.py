from conan import ConanFile


class Project(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def configure(self):
        # We can control the options of our dependencies based on current options
        self.options["catch2"].with_main = True
        self.options["catch2"].with_benchmark = True
        self.options["boost"].header_only = True
        # soci:with_sqlite3=True


    def requirements(self):
        self.requires("boost/1.84.0",force=True)
        self.requires("catch2/2.13.9")
        self.requires("durak/1.0.2",force=True)
        self.requires("confu_soci/0.3.20")
        self.requires("magic_enum/[>=0.9.5 <10]")
        self.requires("range-v3/0.12.0")
        self.requires("certify/cci.20201114")
        self.requires("libsodium/1.0.18")
        self.requires("confu_json/1.0.1")
        self.requires("sml/1.1.5")
        self.requires("corrade/2020.06")
        self.requires("login_matchmaking_game_shared/latest")
        self.requires("modern_durak_game_shared/latest")
        self.requires("modern_durak_game_option/latest")
