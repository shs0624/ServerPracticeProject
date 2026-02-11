//#include <iostream>
//using namespace std;
//
//pair<int, int> arr[41];
//
//int main()
//{
//	arr[0] = { 1,0 };
//	arr[1] = { 0,1 };
//	arr[2] = { 1,1 };
//	for (int i = 3; i <= 40; i++)
//	{
//		arr[i].first = arr[i - 2].first + arr[i - 1].first;
//		arr[i].second = arr[i - 2].second + arr[i - 1].second;
//	}
//	
//	int t, n;
//	cin >> t;
//	for (int i = 0; i < t; i++)
//	{
//		cin >> n;
//		printf("%d %d\n", arr[n].first, arr[n].second);
//	}
//}