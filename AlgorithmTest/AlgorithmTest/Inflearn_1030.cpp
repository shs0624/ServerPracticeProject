#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
using namespace std;

int board[9][9];
int check[9][9];
queue<pair<int, int>> q;
vector<pair<int, int>> vec;
int Size;

void DFS(int L)
{
    if (L == vec.size())
    {
        for (int i = 0; i < 9; i++)
        {
            for (int j = 0; j < 9; j++)
                cout << board[i][j] << " ";
            cout << endl;
        }
        cout << endl;
        exit(0);
    }
    else
    {
        int x = vec[L].first;
        int y = vec[L].second;
        int check[10] = { 0 };

        for (int i = 0; i < 9; i++)
        {
            if (board[i][y] != 0)
                check[board[i][y]] = 1;

            if (board[x][i] != 0)
                check[board[x][i]] = 1;
        }

        int startX = (x / 3) * 3;
        int startY = (y / 3) * 3;
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                if (board[startX + i][startY + j] != 0)
                    check[board[startX + i][startY + j]] = 1;
            }
        }

        for (int i = 1; i <= 9; i++)
        {
            if (check[i] == 0)
            {
                board[x][y] = i;
                DFS(L + 1);
                board[x][y] = 0;
            }
        }
    }
}

void Solution()
{
    DFS(0);
}

int main()
{
    ios_base::sync_with_stdio(false);

    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            cin >> board[i][j];
            if (board[i][j] == 0)
                vec.push_back({ i,j });
        }
    }

    Size = q.size();

    Solution();

    return 0;
}