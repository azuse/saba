#include "Saba/Base/Log.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

class TestSink : public spdlog::sinks::sink
{
public:
	void log(const spdlog::details::log_msg& msg) override
	{
		m_buffer.push_back(msg.formatted.str());
	}

	void flush() override
	{
	}

	std::vector<std::string> m_buffer;
};

TEST(BaseTest, LogTest)
{
	auto logger = mmd::Singleton<mmd::Logger>::Get();
	auto testSink = logger->AddSink<TestSink>();

	EXPECT_NE(nullptr, testSink.get());

	INFO("test1");
	WARN("test2");
	ERROR("test3");

	EXPECT_EQ(3, testSink->m_buffer.size());

	EXPECT_EQ(2, logger->GetLogger()->sinks().size());
	logger->RemoveSink(testSink.get());
	EXPECT_EQ(1, logger->GetLogger()->sinks().size());

	INFO("test4");
	EXPECT_EQ(3, testSink->m_buffer.size());
}
