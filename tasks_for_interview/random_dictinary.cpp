template <K, V>
class RandDict
{
	void put(K k, V v); // O(1)
	V get(K k); // O(1)
	void remove(K k); // O(1)
	K const * random_key(); // O(1)
}