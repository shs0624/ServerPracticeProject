#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

struct Matrix
{
	int r;
	int c;
};

int N;
Matrix _MatrixArr[501];
unsigned int _dp[501];
unsigned int _dpReverse[501];
unsigned int _dpResult[501];
unsigned int _result;

void Solution()
{
	_dp[1] = 0;

	if (N == 1)
	{
		_result = 0;
		return;
	}

	_dp[2] = _MatrixArr[1].r * _MatrixArr[1].c * _MatrixArr[2].c;
	for (int i = 3; i <= N; i++)
	{
		unsigned int oldMulResult = _dp[i - 1] + _MatrixArr[1].r * _MatrixArr[i].r * _MatrixArr[i].c;
		unsigned int newMulResult = _dp[i - 2] + (_MatrixArr[i - 1].r * _MatrixArr[i].r * _MatrixArr[i].c)
			+ (_MatrixArr[1].r * _MatrixArr[i - 2].c * _MatrixArr[i].c);

		if (oldMulResult > newMulResult)
		{
			_dp[i] = newMulResult;
		}
		else
		{
			_dp[i] = oldMulResult;
		}
	}

	_result = _dp[N];

	_dpReverse[N] = 0;
	_dpReverse[N - 1] = _MatrixArr[N - 1].r * _MatrixArr[N].r * _MatrixArr[N].c;
	for (int i = N - 2; i >= 1; i--)
	{
		unsigned int oldMulResult = _dpReverse[i + 1] + _MatrixArr[N].c * _MatrixArr[i].r * _MatrixArr[i].c;
		unsigned int newMulResult = _dpReverse[i + 2] + (_MatrixArr[i + 1].c * _MatrixArr[i].r * _MatrixArr[i].c)
			+ (_MatrixArr[N].c * _MatrixArr[i + 2].r * _MatrixArr[i].c);

		if (oldMulResult > newMulResult)
		{
			_dpReverse[i] = newMulResult;
		}
		else
		{
			_dpReverse[i] = oldMulResult;
		}
	}

	for (int i = 1; i < N; i++)
	{
		_dpResult[i] = _dp[i] + _dpReverse[i + 1];
		_dpResult[i] += _MatrixArr[1].r * _MatrixArr[i].c * _MatrixArr[N].c;

		_result = min(_dpResult[i], _result);
	}

	int b = 5;
}

int main()
{
	ios::sync_with_stdio(false);
	cin.tie(NULL);
	cout.tie(NULL);

	_result = 0xffffffff;

	cin >> N;
	for (int i = 1; i <= N; i++)
	{
		cin >> _MatrixArr[i].r >> _MatrixArr[i].c;
	}

	Solution();

	cout << _result << endl;

	return 0;
}