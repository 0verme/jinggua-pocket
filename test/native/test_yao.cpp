#include "test_framework.h"
#include "test_support.h"

#include "jinggua/domain/yao.h"

void runYaoTests(TestRunner& runner) {
  using jinggua::domain::YaoType;
  using jinggua::domain::YinYang;

  const auto oldYin = jinggua::domain::createYao(1, coinsForTotal(6));
  EXPECT(runner, oldYin.has_value());
  EXPECT_EQ(runner, oldYin->type, YaoType::OldYin);
  EXPECT_EQ(runner, oldYin->yinYang, YinYang::Yin);
  EXPECT_EQ(runner, oldYin->transformedYinYang, YinYang::Yang);
  EXPECT(runner, oldYin->moving);

  const auto youngYang = jinggua::domain::createYao(2, coinsForTotal(7));
  EXPECT(runner, youngYang.has_value());
  EXPECT_EQ(runner, youngYang->type, YaoType::YoungYang);
  EXPECT_EQ(runner, youngYang->yinYang, YinYang::Yang);
  EXPECT_EQ(runner, youngYang->transformedYinYang, YinYang::Yang);
  EXPECT(runner, !youngYang->moving);

  const auto youngYin = jinggua::domain::createYao(3, coinsForTotal(8));
  EXPECT(runner, youngYin.has_value());
  EXPECT_EQ(runner, youngYin->type, YaoType::YoungYin);
  EXPECT_EQ(runner, youngYin->yinYang, YinYang::Yin);
  EXPECT_EQ(runner, youngYin->transformedYinYang, YinYang::Yin);

  const auto oldYang = jinggua::domain::createYao(4, coinsForTotal(9));
  EXPECT(runner, oldYang.has_value());
  EXPECT_EQ(runner, oldYang->type, YaoType::OldYang);
  EXPECT_EQ(runner, oldYang->yinYang, YinYang::Yang);
  EXPECT_EQ(runner, oldYang->transformedYinYang, YinYang::Yin);
  EXPECT(runner, oldYang->moving);

  EXPECT(runner, !jinggua::domain::createYao(0, coinsForTotal(7)).has_value());
  EXPECT(runner, !jinggua::domain::createYao(7, coinsForTotal(7)).has_value());

  // The four coin totals are the complete line conversion table.
  EXPECT_EQ(runner, jinggua::domain::yaoTypeForTotal(6), YaoType::OldYin);
  EXPECT_EQ(runner, jinggua::domain::yaoTypeForTotal(7), YaoType::YoungYang);
  EXPECT_EQ(runner, jinggua::domain::yaoTypeForTotal(8), YaoType::YoungYin);
  EXPECT_EQ(runner, jinggua::domain::yaoTypeForTotal(9), YaoType::OldYang);
  EXPECT_EQ(runner, jinggua::domain::yinYangFor(YaoType::OldYin), YinYang::Yin);
  EXPECT_EQ(runner, jinggua::domain::yinYangFor(YaoType::YoungYang), YinYang::Yang);
  EXPECT_EQ(runner, jinggua::domain::yinYangFor(YaoType::YoungYin), YinYang::Yin);
  EXPECT_EQ(runner, jinggua::domain::yinYangFor(YaoType::OldYang), YinYang::Yang);
  EXPECT_EQ(runner, jinggua::domain::transformedYinYangFor(YaoType::OldYin),
            YinYang::Yang);
  EXPECT_EQ(runner, jinggua::domain::transformedYinYangFor(YaoType::YoungYang),
            YinYang::Yang);
  EXPECT_EQ(runner, jinggua::domain::transformedYinYangFor(YaoType::YoungYin),
            YinYang::Yin);
  EXPECT_EQ(runner, jinggua::domain::transformedYinYangFor(YaoType::OldYang),
            YinYang::Yin);
  EXPECT(runner, jinggua::domain::isMoving(YaoType::OldYin));
  EXPECT(runner, !jinggua::domain::isMoving(YaoType::YoungYang));
  EXPECT(runner, !jinggua::domain::isMoving(YaoType::YoungYin));
  EXPECT(runner, jinggua::domain::isMoving(YaoType::OldYang));
}
