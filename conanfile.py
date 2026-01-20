from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from conan.tools.files import collect_libs, rmdir

class MatchmakingProxy(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators =  "CMakeDeps"

    options = {
        "with_ssl_verification": [True, False],
    }
    default_options = {
        "with_ssl_verification": True
    }

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = False #workaround because this leads to useless options in cmake-tools configure
        tc.variables["WITH_SSL_VERIFICATION"] = self.options.with_ssl_verification
        tc.generate()


    def configure(self):
        # We can control the options of our dependencies based on current options
        self.options["catch2"].with_main = True
        self.options["catch2"].with_benchmark = True
        self.options["my_web_socket"].log_co_spawn_print_exception = True
        self.options["my_web_socket"].log_write = True
        self.options["my_web_socket"].log_read = True
        self.options["my_web_socket"].log_boost_asio = True
        
        


    def requirements(self):
        self.requires("boost/1.90.0", force=True)
        self.requires("confu_soci/[<1]")
        self.requires("magic_enum/0.9.6")
        if self.options.with_ssl_verification:
            self.requires("certify/cci.20201114@modern-durak")
        self.requires("libsodium/1.0.18")
        self.requires("confu_json/1.1.1@modern-durak", force=True)
        self.requires("confu_algorithm/1.2.1")
        self.requires("sml/1.1.11")
        self.requires("login_matchmaking_game_shared/latest")
        self.requires("my_web_socket/3.0.3")
        self.requires("sqlite3/3.44.2")
        self.requires("openssl/3.5.2",force=True)
        ### only for testing please do not put this in the package build recept ###
        
        self.requires("modern_durak_game_shared/latest")
        self.requires("modern_durak_game_option/latest")
        self.requires("catch2/2.13.9")
        ###########################################################################



  

