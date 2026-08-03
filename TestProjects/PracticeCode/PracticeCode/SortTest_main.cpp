//#include <iostream>
//#include <vector>
//using namespace std;
//
//void SelectSort(vector<int>& vec)
//{
//	for (int i = 0; i < vec.size() - 1; i++)
//	{
//		int idx = i;
//		for (int j = i + 1; j < vec.size(); j++)
//		{
//			if (vec[idx] >= vec[j]) idx = j;
//		}
//
//		int temp = vec[i];
//		vec[i] = vec[idx];
//		vec[idx] = temp;
//	}
//}
//
//void InsertionSort(vector<int>& vec)
//{
//	for (int i = 1; i < vec.size(); i++)
//	{
//		int value = vec[i];
//		int j = i - 1;
//		while (j >= 0 && value < vec[j])
//		{
//			vec[j + 1] = vec[j];
//			j--;
//		}
//
//		vec[j + 1] = value;
//	}
//}
//
//void BubbleSort(vector<int>& vec)
//{
//	for (int i = 0; i < vec.size() - 1; i++)
//	{
//		for (int j = 1; j < vec.size() - i; j++)
//		{
//			if (vec[j] < vec[j - 1])
//			{
//				int temp = vec[j];
//				vec[j] = vec[j - 1];
//				vec[j - 1] = temp;
//			}
//		}
//	}
//}
//
//void merge(vector<int>& vec, int s, int m, int e)
//{
//	vector<int> result;
//	int ns = s, nm = m + 1;
//	int idx = 0;
//
//	while (ns <= m && nm <= e)
//	{
//		if (vec[ns] < vec[nm]) result.push_back(vec[ns++]);
//		else result.push_back(vec[nm++]);
//	}
//
//	while (ns <= m) result.push_back(vec[ns++]);
//	while (nm <= e) result.push_back(vec[nm++]);
//
//	for (int i = s; i <= e; i++)
//	{
//		vec[i] = result[idx++];
//	}
//}
//
//void MergeSort(vector<int>& vec, int s, int e)
//{
//	if (s >= e)
//		return;
//
//	int m = (s + e) / 2;
//	MergeSort(vec, s, m);
//	MergeSort(vec, m + 1, e);
//
//	merge(vec, s, m, e);
//}
//
//int QuickSort_Partition(vector<int>& vec, int s, int e)
//{
//	int pivot = vec[e];
//	int ns = s;
//
//	for (int i = s; i < e; i++)
//	{
//		if (vec[i] < pivot)
//		{
//			swap(vec[ns++], vec[i]);
//		}
//	}
//	
//	swap(vec[ns], vec[e]);
//	return ns;
//}
//
//void QuickSort(vector<int>& vec, int s, int e)
//{
//	if (s >= e)
//		return;
//
//	int pivot = QuickSort_Partition(vec, s, e);
//	QuickSort(vec, s, pivot - 1);
//	QuickSort(vec, pivot + 1, e);
//}
//
//int main()
//{
//	vector<int> vec = { 5,10,8,1,3,2,7,7,9,4 };
//
//	//SelectSort(vec);
//	//InsertionSort(vec);
//	//BubbleSort(vec);
//	//MergeSort(vec, 0, vec.size() - 1);
//	QuickSort(vec, 0, vec.size() - 1);
//
//	for (int i = 0; i < vec.size(); i++)
//	{
//		cout << vec[i] << " ";	
//	}
//	cout << endl;
//
//	return 0;
//}