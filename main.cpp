#include <string>
#include <iostream>
#include <thread>

#include "webserver.h"
#include "config/config.h"

#include <nana/gui.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/panel.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/textbox.hpp>
#include <nana/gui/widgets/group.hpp>
#include <nana/threads/pool.hpp>

#include <nana/gui/widgets/menubar.hpp>
#include <nana/gui/place.hpp>
#include <nana/gui/msgbox.hpp>
#include <nana/gui/filebox.hpp>

using namespace nana;

class notepad_form : public form {
  place   place_{*this};
  menubar menubar_{*this};
  textbox textbox_{*this};

public:
  notepad_form() {
    caption("Simple Notepad - Nana C++ Library");
    textbox_.borderless(true);
    API::effects_edge_nimbus(textbox_, effects::edge_nimbus::none);
    textbox_.enable_dropfiles(true);
    textbox_.events().mouse_dropfiles([this](const arg_dropfiles &arg) {
      if (arg.files.size() && _m_ask_save())
        textbox_.load(arg.files.front());
    });
    _m_make_menus();
    place_.div("vert<menubar weight=28><textbox>");
    place_["menubar"] << menubar_;
    place_["textbox"] << textbox_;
    place_.collocate();
    events().unload([this](const arg_unload &arg) {
      if (!_m_ask_save())
        arg.cancel = true;
    });
  }
  textbox &get_tb() { return textbox_; }

private:
  std::filesystem::path _m_pick_file(bool is_open) const {
    filebox fbox(*this, is_open);
    fbox.add_filter("Text", "*.txt");
    fbox.add_filter("All Files", "*.*");
    auto files = fbox.show();
    return (files.empty() ? std::filesystem::path{} : files.front());
  }
  bool _m_ask_save() {
    if (textbox_.edited()) {
      auto   fs = textbox_.filename();
      msgbox box(*this, "Simple Notepad", msgbox::button_t::yes_no_cancel);
      box << "Do you want to save these changes?";
      switch (box.show()) {
      case msgbox::pick_yes:
        if (fs.empty()) {
          fs = _m_pick_file(false);
          if (fs.empty())
            break;
          if (fs.extension().string() != ".txt")
            fs = fs.extension().string() + ".txt";
        }
        textbox_.store(fs);
        break;
      case msgbox::pick_no:
        break;
      case msgbox::pick_cancel:
        return false;
      }
    }
    return true;
  }
  void _m_make_menus() {
    menubar_.push_back("&FILE");
    menubar_.at(0).append("New", [this](menu::item_proxy &ip) {
      if (_m_ask_save())
        textbox_.reset();
    });
    menubar_.at(0).append("Open", [this](menu::item_proxy &ip) {
      if (_m_ask_save()) {
        auto fs = _m_pick_file(true);
        if (!fs.empty())
          textbox_.load(fs);
      }
    });
    menubar_.at(0).append("Save", [this](menu::item_proxy &) {
      auto fs = textbox_.filename();
      if (fs.empty()) {
        fs = _m_pick_file(false);
        if (fs.empty())
          return;
      }
      textbox_.store(fs);
    });
    menubar_.push_back("F&ORMAT");
    menubar_.at(1).append("Line Wrap", [this](menu::item_proxy &ip) { textbox_.line_wrapped(ip.checked()); });
    menubar_.at(1).check_style(0, menu::checks::highlight);
  }
};
void Wait(unsigned wait = 0) {
  if (wait)
    std::this_thread::sleep_for(std::chrono::seconds{wait});
}

int main(int argc, char *argv[]) {
  string sql_database_name = "mydb";
  string user = "rockstar";
  string passwd = "123456";

  Config cfg;
  cfg.parse_arg(argc, argv);

  WebServer server;
  server.init(cfg.PORT, sql_database_name, user, passwd, cfg.sql_num, cfg.LOGWrite, cfg.close_log, cfg.actor_model, cfg.thread_num, cfg.OPT_LINGER,
              cfg.TRIGMode, cfg.LISTENTrigmode, cfg.CONNTrigmode);


  /************************UI************************/
  // Define a form.
  nana::threads::pool thrpool(2);
  form                fm{API::make_center(600, 400)};
  fm.caption("WebServer");
  place plc{fm};
  // the most external widgets
  label out{fm, "This label is out of any group"};
  group left_group{fm, "Config <bold=true, color=blue>Group:</>", true};
  group right_group{fm, "Running <bold=true, color=red>Group:</>", true};


  // Define a button and answer the click event.
  button btn_openserver(fm, nana::rectangle(20, 20, 140, 40));
  btn_openserver.caption("Open server");

  // btn_openserver.size({10, 10, 150, 23});
  // btn_openserver.events().click([&fm, &server] { server.event_loop(); });

  button btn_closeserver{fm, "Close server"};
  btn_closeserver.events().click([&fm, &server] { abort(); });


  label   log_box_label{fm, "Connect"};
  textbox log_box{fm};
  log_box.multi_lines(true);
  log_box.events().text_changed([&plc] {
    plc.collocate();
    std::cout << "button added\n";
  });

  server.sql_pool();


  server.log_write(&log_box);

  server.event_listen();

  btn_openserver.events().click(nana::threads::pool_push(thrpool, [&fm, &server, &log_box] {
    // 先初始化数据库连接池, http服务连接池需要用到数据库连接池
    server.thread_pool();

    server.event_loop();
  }));

  plc.div("vert gap=10 margin=5 <lab weight=30><vertical weight=50% <up_panel>|<weight=25 gap=10 buttons>> | <weight=30 log_box_label> <weight=20% "
          "log_box>");
  plc["lab"] << out;
  plc["up_panel"] << left_group << right_group;
  plc["buttons"] << btn_openserver << btn_closeserver;
  plc["log_box_label"] << log_box_label;
  plc["log_box"] << log_box;

  // the external group contain:
  label label_port{left_group, "Port: "};
  label label_sepcify_path{left_group, "Sepficy the server path: "};

  textbox assign_port{left_group};
  std::setlocale(LC_ALL, "936");
  std::string utf9str = nana::charset("port");
  assign_port.tip_string(utf9str).multi_lines(false);
  assign_port.tip_string("9090");

  textbox assign_filepath{left_group};
  std::setlocale(LC_ALL, "936");
  utf9str = nana::charset("path");
  assign_filepath.tip_string(utf9str).multi_lines(false);

  left_group.div("horizontal gap=3 margin=20  < <vertical <label_above> <label_below> > | 70% <vertical gap=10 arrange=[25,25] <textbox_above> "
                 "<textbox_below>>> ");
  left_group["label_above"] << label_port.text_align(align::right);
  left_group["label_below"] << label_sepcify_path.text_align(align::right);
  left_group["textbox_above"] << assign_port;
  left_group["textbox_below"] << assign_filepath;

  label label_snd{right_group, "Send"};
  label label_rcv{right_group, "Recv"};

  label label_snd_cnt{right_group, "0"};
  label label_rcv_cnt{right_group, "0"};
  right_group.div(
      "horizontal gap=3 margin=20  < <vertical <label_above> <label_below> > | 70% <vertical gap=10 arrange=[25,25] <cnt_above> <cnt_below> > > ");
  right_group["label_above"] << label_snd.text_align(align::right);
  right_group["label_below"] << label_rcv.text_align(align::right);
  right_group["cnt_above"] << label_snd_cnt;
  right_group["cnt_below"] << label_rcv_cnt;

  plc.collocate();

  // Show the form
  fm.show();

  exec();

  return 0;
}