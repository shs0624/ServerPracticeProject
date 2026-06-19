//#include <iostream>
//#include <vector>
//#include <algorithm>
//using namespace std;
//
//int N, result;
//int arr[10];
//
//void Solution()
//{
//	int num = N;
//	while (num > 0)
//	{
//		int n = num % 10;
//		if (n == 9)
//			n = 6;
//
//		arr[n]++;
//		num /= 10;
//	}
//
//	int m = 0;
//	for (int i = 0; i <= 9; i++)
//	{
//		if (i == 6)
//		{
//			if (arr[i] % 2 == 1)
//			{
//				arr[i] += 1;
//			}
//
//			arr[i] = arr[i] / 2;
//		}
//
//		m = max(arr[i], m);
//	}
//
//	result = m;
//
//	return;
//}
//
//int main()
//{
//	ios::sync_with_stdio(false);
//	cin.tie(NULL); 
//	cout.tie(NULL);
//
//	cin >> N;
//
//	Solution();
//
//	cout << result << endl;
//
//	return 0;
//}