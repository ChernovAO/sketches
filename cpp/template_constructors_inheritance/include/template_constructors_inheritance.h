/// Test type aliasin and constructors inheritance

template <typename First, typename Second>
class TwoTypeStorage
{
public:
    TwoTypeStorage(const First& first, const Second& second)
    : m_first(first)
    , m_second(second)
    {
    }
    
protected:
    First m_first;
    Second m_second;
};

template <typename First, typename Second>
class TwoTypeAccessor : public TwoTypeStorage<First, Second>
{
    using Base_t = TwoTypeStorage<First, Second>;
public:
    using Base_t::TwoTypeStorage;

    First getFirst()
    {
        return Base_t::m_first;
    }

    Second getSecond()
    {
        return Base_t::m_second;
    }
};

template <typename First>
class OnlyFirstAccessor : public TwoTypeAccessor<First, int>
{
    using Base_t = TwoTypeAccessor<First, int>;

public:
    OnlyFirstAccessor(const First& first)
    : Base_t(first, 0)
    {
    }
};

class IntFirstAccessor : public OnlyFirstAccessor<int>
{
    using Base_t = OnlyFirstAccessor<int>;
    public:
    using Base_t::OnlyFirstAccessor;
};

