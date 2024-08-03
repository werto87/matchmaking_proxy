from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from conan.tools.files import collect_libs, rmdir

class Project(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators =  "CMakeDeps"

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = False #workaround because this leads to useless options in cmake-tools configure
        tc.generate()


    def configure(self):
        # We can control the options of our dependencies based on current options
        self.options["catch2"].with_main = True
        self.options["catch2"].with_benchmark = True
        self.options["boost"].header_only = True
        # soci:with_sqlite3=True

    def requirements(self):
        self.requires("boost/1.85.0", force=True)
        self.requires("confu_soci/[<1]")
        self.requires("magic_enum/[>=0.9.5 <10]")
        self.requires("certify/cci.20201114")
        self.requires("libsodium/1.0.18")
        self.requires("confu_json/1.1.0", force=True)
        self.requires("sml/1.1.11")
        self.requires("login_matchmaking_game_shared/latest")
        self.requires("my_web_socket/0.0.7")
        
        ### only for testing please do not put this in the package build recept ###
        self.requires("modern_durak_game_option/latest")
        self.requires("catch2/2.13.9")
        ###########################################################################



  

