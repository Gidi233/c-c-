#include "service_ser.hpp"
#include "../Serialization.hpp"
#include "database.hpp"
#include "../Message.hpp"
#include "../event.hpp"
#include "server.hpp"
#include <sys/socket.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
using std::string, std::cout, std::cin, std::endl;

void Getfd(int *fd)
{
    string jso = Recv(*fd);
    string account, password, opposite_account;
    int ID, oppositeID, chatID;
    UserTotal usr;
    Message msg;
    switch (getopt(jso))
    {

    case Register:
        // 判断是否重名
        Get_Info(jso, nullptr, &account, &password, nullptr, nullptr);
        if (Database::User_Exist_Account(account))
        {
            SendInt(*fd, 0);
            break;
        }
        usr = New_User(account, password);
        cout << "账号" << usr.ID << "注册\n";
        SendInt(*fd, usr.ID);
        Database::Set_Account_To_ID(usr.ID, account);
        Database::User_In(usr.ID, To_Json_User(usr));
        server::ID_To_Fd.emplace(usr.ID, *fd);
        break;
    case Login:
        Get_Info(jso, nullptr, &account, &password, nullptr, nullptr);

        if (!Database::User_Exist_Account(account))
        {
            SendInt(*fd, 0); // 不存在
            break;
        }
        usr = From_Json_UserTotal(Database::User_Out(Database::Get_Account_To_ID(account)));
        if (usr.password == password)
        {
            cout << "账号" << usr.ID << "登录\n";
            Change_isLogin_Ser(usr.ID);
            SendInt(*fd, usr.ID);
            server::ID_To_Fd.emplace(usr.ID, *fd);
        }
        else
            SendInt(*fd, 0); // 密码错误

        break;

    case User:
        Get_Info(jso, &ID, nullptr, nullptr, nullptr, nullptr); //
        Send(*fd, To_UserBase(Database::User_Out(ID)), 0);
        break;
    case Frd_List:
        Get_Info(jso, &ID, nullptr, nullptr, nullptr, nullptr); //
        usr = From_Json_UserTotal(Database::User_Out(ID));
        Send(*fd, To_Json_Frdlist(usr.frd), 0);
        cout << "返回用户" << ID << "好友信息\n";
        break;

    case Exit:
        Get_Info(jso, &ID, nullptr, nullptr, nullptr, nullptr);
        Change_isLogin_Ser(ID);
        server::ID_To_Fd.erase(ID);
        cout << "用户" << ID << "退出\n";
        break;

    case Add_Frd:
        Get_Info(jso, &ID, nullptr, nullptr, &oppositeID, nullptr); //
        usr = From_Json_UserTotal(Database::User_Out(ID));
        if (!Database::User_Exist_ID(oppositeID))
        {
            SendInt(*fd, 1);
            break;
        }

        // 检查是否找到了这个值
        if (usr.frd.find(oppositeID) != usr.frd.end())
        {
            SendInt(*fd, 2);
            break;
        }

        chatID = Database::Get_ChatID();
        Database::User_In(ID, Add_Friend(oppositeID, Database::User_Out(ID), chatID));
        Database::User_In(oppositeID, Add_Friend(ID, Database::User_Out(oppositeID), chatID)); // 这些都放到处理好友申请里
        SendInt(*fd, 0);
        break;

    case Del_Frd:
        Get_Info(jso, &ID, nullptr, nullptr, &oppositeID, nullptr); //
        usr = From_Json_UserTotal(Database::User_Out(ID));
        // 检查是否找到了这个值
        if (usr.frd.find(oppositeID) == usr.frd.end())
        {
            SendInt(*fd, 1);
            break;
        }
        // 发送消息
        Database::Del_Chat(usr.frd[oppositeID]); // 构造了删
        usr.frd.erase(oppositeID);
        Database::User_In(ID, To_Json_User(usr));
        { //
            UserTotal opposite = From_Json_UserTotal(Database::User_Out(oppositeID));
            opposite.frd.erase(ID);
            Database::User_In(oppositeID, To_Json_User(opposite));
        }
        SendInt(*fd, 0);
        break;
    case Search_Frd:
        Get_Info(jso, nullptr, nullptr, nullptr, nullptr, &opposite_account); //
        if (Database::User_Exist_Account(opposite_account))
            SendInt(*fd, Database::Get_Account_To_ID(opposite_account));
        else
            SendInt(*fd, 0);
        break;

    case Exist_Frd:
        Get_Info(jso, &ID, nullptr, nullptr, &oppositeID, nullptr); //
        usr = From_Json_UserTotal(Database::User_Out(ID));

        // 检查是否找到了这个值
        if (usr.frd.find(oppositeID) != usr.frd.end())
        {
            SendInt(*fd, 1);
            cout << "存在好友" << oppositeID << endl;
            break;
        }
        SendInt(*fd, 0);
        break;

    case Get_frdChat:
        Get_Info(jso, &ID, nullptr, nullptr, &oppositeID, nullptr); //
        usr = From_Json_UserTotal(Database::User_Out(ID));
        Send(*fd, Database::Chat_Out(usr.frd[oppositeID]), 0);
        cout << "返回" << ID << "和" << oppositeID << "的聊天记录" << endl;
        break;

    case Sendmsg_Tofrd:
        msg = From_Json_Msg(jso);
        usr = From_Json_UserTotal(Database::User_Out(msg.SendID));
        chatID = usr.frd[msg.ReceiveID];
        // 发实时
        if (server::ID_To_Fd.find(msg.ReceiveID) != server::ID_To_Fd.end())
        {
            // cout << *fd << "         " << server::ID_To_Fd[msg.ReceiveID] << endl;
            Send(server::ID_To_Fd[msg.ReceiveID], To_Json_Msg(msg), 1);
        }
        else
        {
            // 加到对方的消息队列
        }
        Database::Chat_In(chatID, Add_Msg(To_Json_Msg(msg), Database::Chat_Out(chatID)));
        cout << "追加" << msg.SendID << "发给" << msg.ReceiveID << "的消息" << endl;
        break;

    default: // jump to case label???
        cout << "啊？" << endl;
        break;
    }
    // close(*fd);
}

void Send(int fd, string jso, bool type)
{
    string realjso = Set_Type(jso, type);
    // 在这里用json设定是实时信息还是回应，多传一个参数
    int numRead = realjso.length();
    char *buffer = new char[numRead + 4];
    memcpy(buffer, &numRead, sizeof(int));
    memcpy(buffer + 4, realjso.c_str(), numRead);
    send(fd, buffer, numRead + 4, 0);
    // cout << send(fd, buffer, numRead + 4, 0) << endl;
}

void SendInt(int fd, int num) //,bool flag
{
    Send(fd, Set_Num(num), 0);
}

string Recv(int fd)
{
    char buffer[10000] = {};
    memset(buffer, 0, sizeof(buffer));
    int reqLen;
    recv(fd, (void *)&reqLen, sizeof(int), 0);

    recv(fd, buffer, reqLen, 0);
    return string(buffer, reqLen);
}

UserTotal New_User(string account, string password)
{
    return UserBase(account, password);
}

bool Change_isLogin_Ser(int ID)
{
    string jso = Change_isLogin(Database::User_Out(ID));
    return Database::User_In(ID, jso);
}