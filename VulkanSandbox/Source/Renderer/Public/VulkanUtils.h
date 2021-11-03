#pragma once

struct VulkanUtils
{
    static bool IsInList(const char* s, const std::vector<const char*>& l)
    {
        for(const auto entry : l)
        {
            bool found = strcmp(entry, s) == 0;
            if (found)
            {
                return true;
            }
        }

        return false;
    }

    static bool IsListSubset(const std::vector<const char*>& l, const std::vector<const char*>& subset_list)
    {
        for (const char* entry : subset_list)
        {
            bool found = IsInList(entry, l);
            if(found == false)
            {
                return false;
            }
        }

        return true;
    }
};
