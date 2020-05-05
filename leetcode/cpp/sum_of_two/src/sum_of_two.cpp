#include "sum_of_two.h"

#include <algorithm>
#include <unordered_map>

namespace leetcode
{
std::vector<int> sum_of_two(std::vector<int>& nums, int target)
{
    std::unordered_map<int, int> rests;
    for (int i = 0; i < static_cast<int>(nums.size()); ++i)
    {
        auto& for_test = nums[i];
        auto it = rests.find(for_test);
        if (rests.end() != it)
        {
            return { std::min(i, it->second), std::max(i, it->second) };;
        }
        rests[target - for_test] = i;
    }
    return {};
}
} // namespace::leetcode

