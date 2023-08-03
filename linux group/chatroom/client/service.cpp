#include "client.hpp"
#include "../Serialization.hpp"
#include "service.hpp"
#include "../Message.hpp"
#include "../event.hpp"
#include <list>
#include <unistd.h>
#include <signal.h>
#include <iostream>
using namespace std;

int Main_Menu_Ser_Register()
{
    string account, password;
    int ID;
    char flag;
    while (1)
    {
        system("clear");
        cout << "账户：";
        cin >> account;
        cout << "密码：";
        cin >> password;
        // 加个再次输入密码
        client::Send(From_Main(Register, account, password));
        if ((ID = client::RecvInt()))
        {
            sigaction(SIGIO, &client::ign, 0);
            cout << "注册成功" << endl;
            sleep(1); //
            return ID;
        }

        else
        {
            cout << "已注册" << endl
                 << "1.重新注册\n0.退出\n";
            cin >> flag;
            if (flag == '0')
                return 0;
        }
    }
}

int Main_Menu_Ser_Login()
{
    string account, password;
    int cnt = 3;
    int ID;
    while (1)
    {
        // system("clear");
        cout << "账户：";
        cin >> account;
        cout << "密码：";
        cin >> password;
        client::Send(From_Main(Login, account, password));
        if ((ID = client::RecvInt()))
        {
            sigaction(SIGIO, &client::ign, 0);
            return ID;
        }
        else
        {
            cnt--;
            if (!cnt)
                return 0;
            cout << "还能尝试" << cnt << "次" << endl;
            continue;
        }
    }
}

UserBase Get_User_Ser(int ID)
{
    client::Send(From_Self(User, ID));
    return From_Json_UserBase(client::Recv());
}

void User_Ser_Exit(int ID)
{
    client::Send(From_Self(Exit, ID));
}

list<Message> Get_ManageList_Ser(int ID)
{
    client::Send(From_Self(Get_ManageList, ID));
    return From_Json_MsgList(To_Manage(client::Recv()));
}

void Manage_Apply_Ser(int ID, list<Message> manage)
{
    int num;
    char choice;
    for (auto &m : manage)
    {
        system("clear");
        m.toString();
        cout << "1.同意\n0.拒绝\n";
        cin >> num;
        client::Send(From_Manage(m.event + 1, ID, m.SendID, num));
        sigaction(SIGIO, &client::respond, 0);
        cout << "1.继续处理\n0.返回\n";
        cin >> choice;
        if (choice == '0')
            break;
    }
}

void Friend_Ser(int ID)
{
    list<UserBase> frd;
    client::Send(From_Self(Frd_List, ID));
    From_Json_Frdlist(client::Recv()); // 把这改了就可以只在Send，Recv里改了
}

void Add_Frd_Ser(int ID)
{
    int frdID;
    char choice;
    while (1)
    {
        system("clear");
        cout << "对方ID:";
        cin >> frdID;
        if (frdID == ID)
        {
            cout << "你小子酒吧进酒吧是吧（添加自己)\n";
            cout << "1.重新输入\n0.返回\n";
            cin >> choice;
            if (choice == '0')
                return;
            else
                continue;
        }
        client::Send(From_Frd(Send_Add_Frd, ID, frdID));
        switch (client::RecvInt())
        {
        case 0:
            cout << "发送申请成功\n";
            sleep(1);
            return;
            break;
        case 1:
            cout << "查无此人\n";
            break;
        case 2:
            cout << "你小子酒吧点炒菜是吧（重复添加)\n";
            break;
        default:
            cout << "啊？" << endl;
            break;
        }
        cout << "1.重新输入\n0.返回\n";
        cin >> choice;
        if (choice == '0')
            return;
    }
}

void Del_Frd_Ser(int ID)
{
    int frdID;
    char choice;
    while (1)
    {
        system("clear");
        cout << "对方ID:";
        cin >> frdID;
        if (frdID == ID)
        {
            cout << "你小子酒吧进酒吧是吧（删自己)\n";
            cout << "1.重新输入\n0.返回\n";
            cin >> choice;
            if (choice == '0')
                return;
            else
                continue;
        }
        client::Send(From_Frd(Del_Frd, ID, frdID));
        switch (client::RecvInt())
        {
        case 0:
            cout << "删除成功\n";
            sleep(1);
            return;
            break;
        case 1:
            cout << "并无该好友\n";
            break;
        default:
            cout << "啊？" << endl;
            break;
        }
        cout << "1.重新输入\n0.返回\n";
        cin >> choice;
        if (choice == '0')
            return;
    }
}

void Send_Msg_Ser(UserBase usr)
{
    int frdID;
    char choice;
    while (1)
    {
        system("clear");
        cout << "对方ID:";
        cin >> frdID;
        if (frdID == usr.ID)
        {
            cout << "你小子跟自己唠是吧\n";
            cout << "1.重新输入\n0.返回\n";
            cin >> choice;
            if (choice == '0')
                return;
            else
                continue;
        }
        client::Send(From_Frd(Exist_Frd, usr.ID, frdID));
        if (client::RecvInt())
        {
            break;
        }
        cout << "并无该好友\n";
        cout << "1.重新输入\n0.返回\n";
        cin >> choice;
        if (choice == '0')
            return;
    }

    client::frdID = frdID;
    client::Send(From_Frd(Get_frdChat, usr.ID, frdID));
    From_Json_Chat(client::Recv());
    string str;
    while (1)
    {
        cin >> str;
        if (str == "\\q")
        {
            client::frdID = -1;
            return;
        }
        cout << "\033[1A\x1b[2K\r";
        Message msg(Sendmsg_Tofrd, usr.ID, usr.account, frdID, str, gettime());
        msg.toString();
        client::Send(To_Json_Msg(msg)); //
        sigaction(SIGIO, &client::respond, 0);
    }
}

void Search_Frd_Ser(int ID)
{
    string account;
    char choice;
    int ans;
    while (1)
    {
        system("clear");
        cout << "对方账户:";
        cin >> account;
        client::Send(From_Frd_Account(Search_Frd, account));
        if ((ans = client::RecvInt()))
        {
            cout << account << "的ID为" << ans << endl;
            sleep(1);
            return;
        }
        cout << "该用户不存在\n";
        cout << "1.重新输入\n0.返回\n";
        cin >> choice;
        if (choice == '0')
            return;
    }
}

void Group_Ser(int ID)
{
    client::Send(From_Self(Grp_List, ID));
}
