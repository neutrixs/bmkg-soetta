#ifndef RENDER_HPP
#define RENDER_HPP

#include <dpp/dpp.h>

namespace commands {
namespace render {
void run(dpp::cluster &bot, const dpp::slashcommand_t &event);
void init();
} // namespace render
} // namespace commands

#endif