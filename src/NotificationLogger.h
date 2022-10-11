#pragma once

namespace NotificationLogger::detail {
    class Proxy {
    public:
        Proxy(const volatile Proxy&) = delete;
        Proxy& operator=(const volatile Proxy&) = delete;

        [[nodiscard]] static Proxy& GetSingleton() noexcept {
            static Proxy singleton;
            return singleton;
        }

        void AddMessage(std::string_view a_message) noexcept {
            const auto _ = std::unique_lock(this->_lock);
            while (this->_notifications.size() >= MAX_BUFFER_SIZE) {
                this->_notifications.pop_back();
            }

            this->_notifications.emplace_front(a_message);
        }

        [[nodiscard]] auto GetMessages() const noexcept -> std::vector<std::string> {
            const auto _ = std::unique_lock(this->_lock);
            return std::vector(this->_notifications.begin(), this->_notifications.end());
        }

    private:
        Proxy() = default;

        static constexpr std::size_t MAX_BUFFER_SIZE = 128;
        mutable std::mutex _lock;
        std::list<std::string> _notifications;
    };

    struct CreateHUDDataMessage {
        static void thunk(RE::HUDData::Type a_type, const char* a_message) {
            if (a_message) {
                auto& proxy = Proxy::GetSingleton();
                proxy.AddMessage(a_message);
            }

            func(a_type, a_message);
        }

        inline static REL::Relocation<decltype(thunk)> func;
    };
}  // namespace NotificationLogger::detail

namespace NotificationLogger::Papyrus {
    inline auto GetCachedMessages(RE::StaticFunctionTag*) -> std::vector<std::string> {
        auto& proxy = detail::Proxy::GetSingleton();
        return proxy.GetMessages();
    }

    inline bool Register(RE::BSScript::Internal::VirtualMachine* a_vm) noexcept {
        assert(a_vm != nullptr);
        a_vm->RegisterFunction("GetCachedMessages"sv, "NotificationLog"sv, GetCachedMessages, true);
        return true;
    }
}  // namespace NotificationLogger::Papyrus

namespace NotificationLogger::Hooks {
    inline void Install() {
        const auto base = RELOCATION_ID(52050, 52933).address();
        const auto target = base + (REL::Module::GetRuntime() != REL::Module::Runtime::AE ? 0x19B : 0x31D);

        auto& trampoline = SKSE::GetTrampoline();
        detail::CreateHUDDataMessage::func = trampoline.write_call<5>(target, detail::CreateHUDDataMessage::thunk);
    }
}  // namespace NotificationLogger::Hooks