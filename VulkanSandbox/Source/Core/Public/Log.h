#pragma once

// This ignores all warnings raised inside External headers
#pragma warning(push, 0)
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#pragma warning(pop)

class Log
{
public:
    static void Init();
    static SharedPtr<spdlog::logger>& GetLogger()
    {
        return logger_;
    }

private:
    static SharedPtr<spdlog::logger> logger_;
};

#define LOG(...)                Log::GetLogger()->info(__VA_ARGS__)
#define LOG_TRACE(...)          Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_WARN(...)           Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)          Log::GetLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...)       Log::GetLogger()->critical(__VA_ARGS__)
