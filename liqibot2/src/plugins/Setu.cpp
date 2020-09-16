#include "Setu.h"

#include <fstream>
#include <direct.h>  
#include <stdio.h>

#include "cmdline.h"
#include "../SubProcess.h"
#include "../Requests.h"

Setu::Setu()
{
}

Setu::Setu(std::vector<Plugin*>* rt_tb_dy_ptr, std::vector<Plugin*>* rt_tb_st_ptr, Permission* permission_ptr)
{
	//��Ҫ��ģ������
	Plugin::name = "Setu";

	this->rt_tb_dy_ptr = rt_tb_dy_ptr;
	this->rt_tb_st_ptr = rt_tb_st_ptr;
	this->permission_ptr = permission_ptr;

	//Ĭ��config
	std::string conf = u8R""(
		{
			"active": true,
			"triggers": [
				"ɫͼ"
				],
			"max-QPS": 20
		}
		)"";

	config = parseJson(conf);
	loadConfig();
}

Setu::~Setu()
{
}

float Setu::metric(Message msg)
{
	//��ɫͼ
	//�ж��Ƿ���ɫͼ������
	if (config["active"].asBool())
	{
		if (msg.type == Message::FriendMessage || msg.type == Message::GroupMessage)
		{
			std::string s = msg.msgChain.toString();

			for (int i = 0; i < config["triggers"].size(); i++)
			{
				std::string command = config["triggers"][i].asString();
				//std::cout << s << "\n" << command << "\n" << (s == command) << "\n";
				if (s == command)
				{
					return 1.0;
				}
			}
		}
	}
	return 0.0f;
}

void Setu::run(Message msg, QQApi* qqApi_ptr)
{
	//�ж��Ƿ���ɫͼ������
	if (msg.type == Message::FriendMessage || msg.type == Message::GroupMessage)
	{
		std::string s = msg.msgChain.toString();

		for (int i = 0; i < config["triggers"].size(); i++)
		{
			std::string command = config["triggers"][i].asString();
			//std::cout << s << "\n" << command << "\n" << (s == command) << "\n";
			if (s == command)
			{
				//��ɫͼ

				//����SelectSetu.py���������·�����ļ������Ի��зָ�
				std::string ans = SubProcess::popen("python data/plugins/Setu/SelectSetu.py");

				//����ͼƬ��Ϣ
				auto imgfilelist = splitString(ans, "\n");
				MessageChain mc;
				for (auto imgfile : imgfilelist)
				{
					MessageChain::AMessage a;
					a.type = MessageChain::AMessage::Image;
					a.path = imgfile;
					
					mc.chain.push_back(a);
				}

				//������Ϣ
				qqApi_ptr->sendMessage(msg.member, 0, mc);

				return ;
			}
		}
	}

	//����ɫͼ�����ʣ�ִ��ɫͼ���
	//������Ϣ��
	for (int i = 0; i < msg.msgChain.chain.size(); i++)
	{
		if (msg.msgChain.chain[i].type == MessageChain::AMessage::Image)
		{
			//��ȡurl
			std::string url = msg.msgChain.chain[i].url;
			//��ȡimageId
			std::string imageId = msg.msgChain.chain[i].imageId;
			//����CheckSetu.py������url��imgid
			std::string ans = SubProcess::popen("python data/plugins/Setu/CheckSetu.py \"" + url + "\" " + imageId);
			if (ans.size() > 0)
			{
				qqApi_ptr->sendMessage(msg.member, 0, ansi_to_utf8(ans));
			}
		}
	}
	

}

void Setu::onCommand(Message msg, std::string s, QQApi* qqApi_ptr)
{
	/*
		example: plugins/Setu -c add -t ɫͼ��
				 plugins/ScoldMe -c add -i [ͼƬ]
				 /plugins/ScoldMe -c get
				 plugins/ScoldMe -c get --json
				 plugins/ScoldMe -c del -t ���ң�
				 plugins/ScoldMe -c del -i [ͼƬ]
	*/

	cmdline::parser p;

	p.add<int>("active", 'a', "global repeat active, 1 for enable, 0 for disable", false, 1);
	p.add<std::string>("cmd", 'c', "add, get or del", true, "", cmdline::oneof<std::string>("add", "get", "del"));
	p.add("json", 'j', "raw json config, only work with \"-c get\"");
	p.add<std::string>("trigger", 't', "trigger words", false, "");
	p.add<std::string>("image", 'i', "setu", false, "");

	try
	{
		p.parse_check(s);
	}
	catch (const std::string& e)
	{
		qqApi_ptr->sendMessage(msg.member, 0, e);
		return;
	}

	config["active"] = p.get<int>("active") ? true : false;

	switch (stringhash_run_time(p.get<std::string>("cmd").c_str()))
	{
	case "add"_hash:
		if (permission_ptr->getRole(0, msg.member.id) <= Permission::DefaultRole)
		{
			qqApi_ptr->sendMessage(msg.member, 0,
				u8"����ʧ�ܣ�Ȩ�޲���"
			);
			break;
		}
		if (!p.get<std::string>("trigger").empty())
		{
			config["triggers"].append(p.get<std::string>("trigger"));
		}
		if (!p.get<std::string>("image").empty())
		{
			std::string b64 = p.get<std::string>("reply");
			MessageChain mc = MessageChain::fromString(b64);
			if (mc.chain[0].type == MessageChain::AMessage::Image)
			{
				//����ͼƬ
				Requests r = Requests::get(mc.chain[0].url);

				std::ofstream outfile(mc.chain[0].id + "." + r.content_type.substr(6));
				outfile << r.text;
				outfile.close();
			}
		}
		qqApi_ptr->sendMessage(msg.member, 0,
			"set success"
		);
		saveConfig();
		break;

	case "get"_hash:
		if (true)
		{
			qqApi_ptr->sendMessage(msg.member, 0,
				dumpsJson(config)
			);
			break;
		}
		break;

	case "del"_hash:
		if (permission_ptr->getRole(0, msg.member.id) <= Permission::DefaultRole)
		{
			qqApi_ptr->sendMessage(msg.member, 0,
				"����ʧ�ܣ�Ȩ�޲���"
			);
			break;
		}
		if (p.get<std::string>("trigger").size())
		{
			Json::Value v;
			for (int i = 0; i < config["triggers"].size(); i++)
			{
				if (config["triggers"][i].asString() == p.get<std::string>("trigger"))
				{
					config["triggers"].removeIndex(i, &v);
					qqApi_ptr->sendMessage(msg.member, 0,
						"del trigger word " + v.asString() + " success"
					);
					break;
				}
			}
			if (v.empty())
			{
				qqApi_ptr->sendMessage(msg.member, 0,
					"trigger word " + p.get<std::string>("trigger") + " not found"
				);
			}
		}
		if (p.get<std::string>("image").size())
		{
			qqApi_ptr->sendMessage(msg.member, 0,
				"�˹���δ���"
			);
		}
		saveConfig();
		break;

	default:
		break;
	}

}

void Setu::onClose()
{
}
