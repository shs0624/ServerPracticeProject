//#include <iostream>
//#include <stack>
//using namespace std;
//
//void Solution(int N, int* arr, int* result)
//{
//    stack<int> st;
//
//    for (int i = N - 1; i >= 0; i--)
//    {
//        while (!st.empty())
//        {
//            if (arr[st.top()] < arr[i])
//            {
//                result[st.top()] = i + 1;
//                st.pop();
//            }
//            else
//            {
//                break;
//            }
//        }
//
//        st.push(i);
//    }
//
//    for (int i = 0; i < N; i++)
//    {
//        cout << result[i];
//        if (i < N - 1)
//        {
//            cout << " ";
//        }
//    }
//}
//
//int main() {
//    ios_base::sync_with_stdio(false);
//
//    int N;
//    int arr[100001];
//    int result[100001] = { 0, };
//
//    cin >> N;
//    cin.ignore();
//    for (int i = 0; i < N; i++)
//    {
//        cin >> arr[i];
//    }
//
//    Solution(N, arr, result);
//
//    return 0;
//}