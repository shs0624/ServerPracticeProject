//#include <string>
//#include <vector>
//#include <iostream>
//using namespace std;
//
//void dp(vector<int>& result, vector<vector<int>>& vec, int startX, int startY, int length)
//{
//    int num = vec[startX][startY];
//
//    for (int i = startX; i < startX + length; i++)
//    {
//        for (int j = startY; j < startY + length; j++)
//        {
//            if (vec[i][j] != num)
//            {
//                // dp
//                int nLength = length / 2;
//                if (nLength >= 1)
//                {
//                    dp(result, vec, startX, startY, nLength);
//                    dp(result, vec, startX + nLength, startY, nLength);
//                    dp(result, vec, startX, startY + nLength, nLength);
//                    dp(result, vec, startX + nLength, startY + nLength, nLength);
//                    return;
//                }
//                else
//                    break;
//            }
//        }
//    }
//
//    result[num]++;
//}
//
//vector<int> solution(vector<vector<int>> arr) {
//    vector<int> answer = { 0,0 };
//
//    dp(answer, arr, 0, 0, arr.size());
//
//    return answer;
//}
//
//void main()
//{
//    vector<vector<int>> arr = { {1,1,0,0,}, {1,0,0,0}, {1,0,0,1},{1,1,1,1} };
//
//    vector<int> result = solution(arr);
//
//    cout << result[0] << " / " << result[1] << endl;
//
//    return;
//}