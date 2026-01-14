#include <iostream>
#include <memory>
#include <functional>
#include <mutex>
#include <string>

// --- CORE DATABASE ---
class CoreDatabase {
    // Private type for internal use only
    struct SpecificLock {
        std::lock_guard<std::mutex> lock;
        SpecificLock(std::mutex& m) : lock(m) { std::cout << "[Lock] DB-Internal.\n"; }
    };

    std::mutex mtx;
    void* conn = (void*)0xDB001;
    bool valid = false;

public:
    // FIX: Define Extension HERE so it is a complete type for inheritance
    struct Extension {
        virtual ~Extension() = default;
        virtual bool init() = 0;
    };

    std::unique_ptr<Extension> extension;

    // The Nested Update: Inheritable and has access to SpecificLock
    struct Update {
        CoreDatabase& db;
        explicit Update(CoreDatabase& d) : db(d) {}

        void insertUsers(const std::string& name) {
            SpecificLock lock(db.mtx); 
            std::cout << "[Core] " << name << " inserted via " << db.conn << "\n";
        }

        // Bridge to Extension: Uses templates to avoid downcasting ctx
        template<typename TExtUpdate>
        void withExtension(std::function<void(TExtUpdate&)> extensionLogic) {
            if (db.extension) {
                TExtUpdate extCtx(db); // Instantiates the specific derived Update
                extensionLogic(extCtx);
            }
        }
    };

    // Flag-only initialization
    void init(bool enableCars);

    void update(std::function<void(Update&)> callback) {
        if (!valid) throw std::runtime_error("DB Invalid");
        Update ctx(*this);
        callback(ctx);
    }
};

// --- THE EXTENSION ---
// Now valid because CoreDatabase::Extension is fully defined
struct CarsExtension : public CoreDatabase::Extension {
    struct Update : public CoreDatabase::Update {
        using CoreDatabase::Update::Update;

        void insertCars(const std::string& model, int year) {
            insertUsers("Owner_" + model); // Calls Core method
            std::cout << "[Cars] Adding " << model << " (" << year << ")\n";
        }
    };

    bool init() override { return true; }
};

// --- CORE IMPLEMENTATION ---
void CoreDatabase::init(bool enableCars) {
    if (enableCars) {
        extension = std::make_unique<CarsExtension>();
        if (!extension->init()) { valid = false; return; }
    }
    valid = true;
}

// --- USAGE FROM MAIN ---
int main() {
    CoreDatabase db;
    db.init(true); // Flag-only init

    db.update([](CoreDatabase::Update& ctx) {
        ctx.insertUsers("Admin"); 

        // Access extension logic with fields, no downcasting
        ctx.withExtension<CarsExtension::Update>([](CarsExtension::Update& cars) {
            cars.insertCars("Tesla Model S", 2026);
        });
    });

    return 0;
}
