#include	"primes.h"

//
// see http://stackoverflow.com/questions/4475996/given-prime-number-n-compute-the-next-prime
//

// ----------------------------------------------------------------------------

static bool IsPrime(const int x)
{
	int o = 4;
	int i = 5;
	while (true)
	{
		const int q = x / i;
		if (q < i)
		{
			return true;
		}

		if (x == (q * i))
		{
			return false;
		}

		o ^= 6;
		i += o;
	}
}
// ----------------------------------------------------------------------------

int FindNextPrime(const int n)
{
	if (n <= 2)
	{
		return 2;
	}
	else if (n == 3)
	{
		return 3;
	}
	else if (n <= 5)
	{
		return 5;
	}

	int k = n / 6;
	int i = n - (6 * k);
	int o = i < 2 ? 1 : 5;
	int x = (6 * k) + o;

	for (i = (3 + o) / 2; !IsPrime(x); x += i)
	{
		i ^= 6;
	}

	return x;
}

// ----------------------------------------------------------------------------

