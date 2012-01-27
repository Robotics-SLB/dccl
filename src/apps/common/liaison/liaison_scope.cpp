// copyright 2011 t. schneider tes@mit.edu
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.

#include "liaison_scope.h"

#include <Wt/WStandardItem>
#include <Wt/WPanel>
#include <Wt/WTextArea>
#include <Wt/WAnchor>
#include <Wt/WLineEdit>
#include <Wt/WPushButton>
#include <Wt/WVBoxLayout>
#include <Wt/WStringListModel>
#include <Wt/WSortFilterProxyModel>
#include <Wt/WSelectionBox>
#include <Wt/WTable>
#include <Wt/WTimer>
#include <Wt/Chart/WCartesianChart>
#include <Wt/WDateTime>
#include "liaison.h"
#include "goby/moos/moos_protobuf_helpers.h"

#include <boost/regex.hpp>
#include <boost/algorithm/string/regex.hpp>

using namespace Wt;
using namespace goby::common::logger_lock;
using namespace goby::common::logger;

goby::common::LiaisonScope::LiaisonScope(ZeroMQService* zeromq_service,
                                         WTimer* timer, Wt::WContainerWidget* parent)
    : LiaisonContainer(parent),
      MOOSNode(zeromq_service),
      zeromq_service_(zeromq_service),
      moos_scope_config_(Liaison::cfg_.GetExtension(protobuf::moos_scope_config)),
      history_model_(new Wt::WStringListModel(this)),
      model_(new LiaisonScopeMOOSModel(moos_scope_config_, this)),
      proxy_(new Wt::WSortFilterProxyModel(this)),
      main_layout_(new Wt::WVBoxLayout(this)),
      controls_div_(new ControlsContainer(timer)),
      subscriptions_div_(new SubscriptionsContainer(this, model_, history_model_, msg_map_)),
      history_header_div_(new HistoryContainer(this, main_layout_, history_model_, moos_scope_config_)),
      regex_filter_div_(new RegexFilterContainer(model_, proxy_, moos_scope_config_)),
      scope_tree_view_(new LiaisonScopeMOOSTreeView(moos_scope_config_)),
      bottom_fill_(new WContainerWidget)
{
    protobuf::ZeroMQServiceConfig ipc_sockets;
    protobuf::ZeroMQServiceConfig::Socket* internal_subscribe_socket = ipc_sockets.add_socket();
    internal_subscribe_socket->set_socket_type(protobuf::ZeroMQServiceConfig::Socket::SUBSCRIBE);
    internal_subscribe_socket->set_socket_id(Liaison::LIAISON_INTERNAL_SCOPE_SUBSCRIBE_SOCKET);
    internal_subscribe_socket->set_transport(protobuf::ZeroMQServiceConfig::Socket::INPROC);
    internal_subscribe_socket->set_connect_or_bind(protobuf::ZeroMQServiceConfig::Socket::CONNECT);
    internal_subscribe_socket->set_socket_name(Liaison::LIAISON_INTERNAL_PUBLISH_SOCKET_NAME);

    protobuf::ZeroMQServiceConfig::Socket* internal_publish_socket = ipc_sockets.add_socket();
    internal_publish_socket->set_socket_type(protobuf::ZeroMQServiceConfig::Socket::PUBLISH);
    internal_publish_socket->set_socket_id(Liaison::LIAISON_INTERNAL_SCOPE_PUBLISH_SOCKET);
    internal_publish_socket->set_transport(protobuf::ZeroMQServiceConfig::Socket::INPROC);
    internal_publish_socket->set_connect_or_bind(protobuf::ZeroMQServiceConfig::Socket::CONNECT);
    internal_publish_socket->set_socket_name(Liaison::LIAISON_INTERNAL_SUBSCRIBE_SOCKET_NAME);

    zeromq_service_->merge_cfg(ipc_sockets);    



    this->resize(WLength::Auto, WLength(100, WLength::Percentage));
    
    zeromq_service_->socket_from_id(Liaison::LIAISON_INTERNAL_SCOPE_SUBSCRIBE_SOCKET).set_global_blackout(boost::posix_time::milliseconds(1/Liaison::cfg_.update_freq()*1e3));    

    setStyleClass("scope");

    proxy_->setSourceModel(model_);
    scope_tree_view_->setModel(proxy_);    
    scope_tree_view_->sortByColumn(moos_scope_config_.sort_by_column(),
                                   moos_scope_config_.sort_ascending() ?
                                   AscendingOrder : DescendingOrder);

    
    main_layout_->addWidget(controls_div_);
    main_layout_->addWidget(subscriptions_div_);
    main_layout_->addWidget(history_header_div_);
    main_layout_->addWidget(regex_filter_div_);
    main_layout_->addWidget(scope_tree_view_);
    main_layout_->setResizable(main_layout_->count()-1);    
    main_layout_->addWidget(bottom_fill_, -1, AlignTop);
    main_layout_->addStretch(1);
    bottom_fill_->resize(WLength::Auto, 100);
    
    for(int i = 0, n = moos_scope_config_.subscription_size(); i < n; ++i)
        subscriptions_div_->add_subscription(moos_scope_config_.subscription(i));

    for(int i = 0, n = moos_scope_config_.history_size(); i < n; ++i)
        history_header_div_->add_history(moos_scope_config_.history(i));
    
    
    wApp->globalKeyPressed().connect(this, &LiaisonScope::handle_global_key);
}

void goby::common::LiaisonScope::loop()
{
    glog.is(DEBUG2, lock) && glog << "LiaisonScope: polling" << std::endl << unlock;
    while(zeromq_service_->poll(0))
    { }
}


void goby::common::LiaisonScope::attach_pb_rows(const std::vector<Wt::WStandardItem *>& items,
                                                CMOOSMsg& msg)
{
    const std::string& value = msg.GetString();
    boost::shared_ptr<google::protobuf::Message> pb_msg =
        dynamic_parse_for_moos(value);

    Wt::WStandardItem* key_item = items[protobuf::MOOSScopeConfig::COLUMN_KEY];
    
    
    if(pb_msg)
    {
        
        std::vector<std::string> result;
        std::string debug_string = pb_msg->DebugString();
        boost::trim(debug_string);
            
        boost::split(result, debug_string, boost::is_any_of("\n"));

        key_item->setRowCount(result.size());
        key_item->setColumnCount(protobuf::MOOSScopeConfig::COLUMN_MAX + 1);


        items[protobuf::MOOSScopeConfig::COLUMN_TYPE]->setText(pb_msg->GetDescriptor()->full_name() + " (Protobuf)");
        items[protobuf::MOOSScopeConfig::COLUMN_VALUE]->setText(pb_msg->ShortDebugString());

        for(int i = 0, n = result.size(); i < n; ++i)
        {
            for(int j = 0; j <= protobuf::MOOSScopeConfig::COLUMN_MAX; ++j)
            {
                if(!key_item->child(i, j))
                    key_item->setChild(i, j, new Wt::WStandardItem);

                if(j == protobuf::MOOSScopeConfig::COLUMN_VALUE)
                {
                    key_item->child(i,j)->setText(result[i]);
                }
                else
                {
                    // so we can still sort by these fields
                    key_item->child(i, j)->setText(items[j]->text());
                    key_item->child(i, j)->setStyleClass("invisible");    
                }
            }
        }
    }
}
std::vector<Wt::WStandardItem *> goby::common::LiaisonScope::create_row(CMOOSMsg& msg)
{
    std::vector<Wt::WStandardItem *> items;
    for(int i = 0; i <= protobuf::MOOSScopeConfig::COLUMN_MAX; ++i)
        items.push_back(new WStandardItem);
    update_row(msg, items);

    return items;
}



void goby::common::LiaisonScope::update_row(CMOOSMsg& msg, const std::vector<WStandardItem *>& items)
{
    items[protobuf::MOOSScopeConfig::COLUMN_KEY]->setText(msg.GetKey());

    items[protobuf::MOOSScopeConfig::COLUMN_TYPE]->setText((msg.IsDouble() ? "double" : "string"));
        
    if(msg.IsDouble())
        items[protobuf::MOOSScopeConfig::COLUMN_VALUE]->setData(msg.GetDouble(), DisplayRole);
    else
        items[protobuf::MOOSScopeConfig::COLUMN_VALUE]->setData(msg.GetString(), DisplayRole);

    
    items[protobuf::MOOSScopeConfig::COLUMN_TIME]->setData(WDateTime::fromPosixTime(goby::util::unix_double2ptime(msg.GetTime())), DisplayRole);
    
    items[protobuf::MOOSScopeConfig::COLUMN_COMMUNITY]->setText(msg.GetCommunity());
    items[protobuf::MOOSScopeConfig::COLUMN_SOURCE]->setText(msg.m_sSrc);
    items[protobuf::MOOSScopeConfig::COLUMN_SOURCE_AUX]->setText(msg.GetSourceAux());
    
    if(msg.IsString())
        attach_pb_rows(items, msg);


    
}

void goby::common::LiaisonScope::handle_global_key(Wt::WKeyEvent event)
{
    while(zeromq_service_->poll(0))
    { }
}




void goby::common::LiaisonScope::moos_inbox(CMOOSMsg& msg)
{

    using goby::moos::operator<<;
    
    glog.is(DEBUG1, lock) && glog << "LiaisonScope: got message:  " << msg << std::endl << unlock;
    std::map<std::string, int>::iterator it = msg_map_.find(msg.GetKey());
    if(it != msg_map_.end())
    {
        std::vector<WStandardItem*> items;
        items.push_back(model_->item(it->second, protobuf::MOOSScopeConfig::COLUMN_KEY));
        items.push_back(model_->item(it->second, protobuf::MOOSScopeConfig::COLUMN_TYPE));
        items.push_back(model_->item(it->second, protobuf::MOOSScopeConfig::COLUMN_VALUE));
        items.push_back(model_->item(it->second, protobuf::MOOSScopeConfig::COLUMN_TIME));
        items.push_back(model_->item(it->second, protobuf::MOOSScopeConfig::COLUMN_COMMUNITY));
        items.push_back(model_->item(it->second, protobuf::MOOSScopeConfig::COLUMN_SOURCE));
        items.push_back(model_->item(it->second, protobuf::MOOSScopeConfig::COLUMN_SOURCE_AUX));
        update_row(msg, items);
    }
    else
    {
        std::vector<WStandardItem*> items = create_row(msg);
        msg_map_.insert(make_pair(msg.GetKey(), model_->rowCount()));
        model_->appendRow(items);
        history_model_->addString(msg.GetKey());
        history_model_->sort(0);
        regex_filter_div_->handle_set_regex_filter();
    }

    std::map<std::string, HistoryContainer::MVC>::iterator hist_it = history_header_div_->history_models_.find(msg.GetKey());
    if(hist_it != history_header_div_->history_models_.end())
    {
        hist_it->second.model->appendRow(create_row(msg));
        hist_it->second.proxy->setFilterRegExp(".*");
    }
}


// void goby::common::LiaisonScope::expanded(Wt::WModelIndex index)
// {
//     MOOSNode::set_blackout(boost::any_cast<Wt::WString>(index.data()).narrow(),
//                            boost::posix_time::milliseconds(0));
// }

// void goby::common::LiaisonScope::collapsed(Wt::WModelIndex index)
// {
//     MOOSNode::clear_blackout(boost::any_cast<Wt::WString>(index.data()).narrow());    
// }



goby::common::LiaisonScopeMOOSTreeView::LiaisonScopeMOOSTreeView(const protobuf::MOOSScopeConfig& moos_scope_config , Wt::WContainerWidget* parent /*= 0*/)
    : WTreeView(parent)
{
    this->setAlternatingRowColors(true);

    this->setColumnWidth(protobuf::MOOSScopeConfig::COLUMN_KEY, moos_scope_config.column_width().key_width());
    this->setColumnWidth(protobuf::MOOSScopeConfig::COLUMN_TYPE, moos_scope_config.column_width().type_width());
    this->setColumnWidth(protobuf::MOOSScopeConfig::COLUMN_VALUE, moos_scope_config.column_width().value_width());
    this->setColumnWidth(protobuf::MOOSScopeConfig::COLUMN_TIME, moos_scope_config.column_width().time_width());
    this->setColumnWidth(protobuf::MOOSScopeConfig::COLUMN_COMMUNITY, moos_scope_config.column_width().community_width());
    this->setColumnWidth(protobuf::MOOSScopeConfig::COLUMN_SOURCE, moos_scope_config.column_width().source_width());
    this->setColumnWidth(protobuf::MOOSScopeConfig::COLUMN_SOURCE_AUX, moos_scope_config.column_width().source_aux_width());

    this->resize(Wt::WLength::Auto,
                 moos_scope_config.scope_height());

    this->setMinimumSize(moos_scope_config.column_width().key_width()+
                         moos_scope_config.column_width().type_width()+
                         moos_scope_config.column_width().value_width()+
                         moos_scope_config.column_width().time_width()+
                         moos_scope_config.column_width().community_width()+
                         moos_scope_config.column_width().source_width()+
                         moos_scope_config.column_width().source_aux_width()+
                         7*(protobuf::MOOSScopeConfig::COLUMN_MAX+1),
                         Wt::WLength::Auto);    

//    this->doubleClicked().connect(this, &LiaisonScopeMOOSTreeView::handle_double_click);
}

 // void goby::common::LiaisonScopeMOOSTreeView::handle_double_click(const Wt::WModelIndex& proxy_index, const Wt::WMouseEvent& event)
// {

     
//     const Wt::WAbstractProxyModel* proxy = dynamic_cast<const Wt::WAbstractProxyModel*>(this->model());
//     const Wt::WStandardItemModel* model = dynamic_cast<Wt::WStandardItemModel*>(proxy->sourceModel());
//     WModelIndex model_index = proxy->mapToSource(proxy_index);

//     glog.is(DEBUG1, lock) && glog << "clicked: " << model_index.row() << "," << model_index.column() << std::endl << unlock;    
    
//     attach_pb_rows(model->item(model_index.row(), protobuf::MOOSScopeConfig::COLUMN_KEY),
//                    model->item(model_index.row(), protobuf::MOOSScopeConfig::COLUMN_VALUE)->text().narrow());

//     this->setExpanded(proxy_index, true);
// }





goby::common::LiaisonScopeMOOSModel::LiaisonScopeMOOSModel(const protobuf::MOOSScopeConfig& moos_scope_config, Wt::WContainerWidget* parent /*= 0*/)
    : WStandardItemModel(0, protobuf::MOOSScopeConfig::COLUMN_MAX+1, parent)
{
    this->setHeaderData(protobuf::MOOSScopeConfig::COLUMN_KEY, Horizontal, std::string("Key"));
    this->setHeaderData(protobuf::MOOSScopeConfig::COLUMN_TYPE, Horizontal, std::string("Type"));
    this->setHeaderData(protobuf::MOOSScopeConfig::COLUMN_VALUE, Horizontal, std::string("Value"));
    this->setHeaderData(protobuf::MOOSScopeConfig::COLUMN_TIME, Horizontal, std::string("Time"));
    this->setHeaderData(protobuf::MOOSScopeConfig::COLUMN_COMMUNITY, Horizontal, std::string("Community"));
    this->setHeaderData(protobuf::MOOSScopeConfig::COLUMN_SOURCE, Horizontal, std::string("Source"));
    this->setHeaderData(protobuf::MOOSScopeConfig::COLUMN_SOURCE_AUX, Horizontal, std::string("Source Aux"));


}


goby::common::LiaisonScope::ControlsContainer::ControlsContainer(Wt::WTimer* timer,
                                                                 Wt::WContainerWidget* parent /*= 0*/)
    : Wt::WContainerWidget(parent),
      timer_(timer),
      play_pause_button_(new WPushButton("Play/Pause", this)),
      play_state_(new Wt::WText(this))
{
    play_pause_button_->clicked().connect(boost::bind(&ControlsContainer::handle_play_pause, this, true));

    handle_play_pause(false);
}

void goby::common::LiaisonScope::ControlsContainer::handle_play_pause(bool toggle_state)
{
    bool is_paused = !timer_->isActive();
    if(toggle_state)
        is_paused = !is_paused;

    is_paused ? timer_->stop() : timer_->start();
    play_state_->setText(is_paused ? "Paused (any key refreshes). " : "Playing... ");
}

goby::common::LiaisonScope::SubscriptionsContainer::SubscriptionsContainer(
    MOOSNode* node,
    Wt::WStandardItemModel* model,
    Wt::WStringListModel* history_model,
    std::map<std::string, int>& msg_map,
    Wt::WContainerWidget* parent /*= 0*/)
    : WContainerWidget(parent),
      node_(node),
      model_(model),
      history_model_(history_model),
      msg_map_(msg_map),
      add_text_(new WText("Add subscription (e.g. NAV* or NAV_X): ", this)),
      subscribe_filter_text_(new WLineEdit(this)),
      subscribe_filter_button_(new WPushButton("Apply", this)),
      subscribe_break_(new WBreak(this)),
      remove_text_(new WText("Subscriptions (click to remove): ", this))
{
    subscribe_filter_button_->clicked().connect(this, &SubscriptionsContainer::handle_add_subscription);
    subscribe_filter_text_->enterPressed().connect(this, &SubscriptionsContainer::handle_add_subscription);
}


void goby::common::LiaisonScope::SubscriptionsContainer::handle_add_subscription()
{    
    add_subscription(subscribe_filter_text_->text().narrow());
    subscribe_filter_text_->setText("");
}

void goby::common::LiaisonScope::SubscriptionsContainer::add_subscription(std::string type)
{
    boost::trim(type);
    if(type.empty())
        return;
    
    WPushButton* new_button = new WPushButton(this);

    new_button->setText(type + " ");
    node_->subscribe(type, Liaison::LIAISON_INTERNAL_SCOPE_SUBSCRIBE_SOCKET);

    new_button->clicked().connect(boost::bind(&SubscriptionsContainer::handle_remove_subscription, this, new_button));
}



void goby::common::LiaisonScope::SubscriptionsContainer::handle_remove_subscription(WPushButton* clicked_button)
{
    std::string type_name = clicked_button->text().narrow();
    boost::trim(type_name);
    unsigned type_name_size = type_name.size();
    
    node_->unsubscribe(clicked_button->text().narrow(), Liaison::LIAISON_INTERNAL_SCOPE_SUBSCRIBE_SOCKET);

    bool has_wildcard_ending = (type_name[type_name_size - 1] == '*');
    if(has_wildcard_ending)
        type_name = type_name.substr(0, type_name_size-1);

    for(int i = model_->rowCount()-1, n = 0; i >= n; --i)
    {
        std::string text_to_match = model_->item(i, 0)->text().narrow();
        boost::trim(text_to_match);
        
        bool remove = false;
        if(has_wildcard_ending && boost::starts_with(text_to_match, type_name))
            remove = true;
        else if(!has_wildcard_ending && boost::equals(text_to_match, type_name))
            remove = true;

        if(remove)
        {            
            history_model_->removeRows(msg_map_[text_to_match], 1);
            msg_map_.erase(text_to_match);
            glog.is(DEBUG1, lock) && glog << "LiaisonScope: removed " << text_to_match << std::endl << unlock;            
            model_->removeRow(i);
            
            // shift down the remaining indices
            for(std::map<std::string, int>::iterator it = msg_map_.begin(),
                    n = msg_map_.end();
                it != n; ++it)
            {
                if(it->second > i)
                    --it->second;
            }            
        }
    }


    this->removeWidget(clicked_button);
    delete clicked_button; // removeWidget does not delete
}

goby::common::LiaisonScope::HistoryContainer::HistoryContainer(MOOSNode* node,
                                                               Wt::WVBoxLayout* main_layout,
                                                               Wt::WAbstractItemModel* model,
                                                               const protobuf::MOOSScopeConfig& moos_scope_config,
                                                               Wt::WContainerWidget* parent /* = 0 */)
    : Wt::WContainerWidget(parent),
      node_(node),
      main_layout_(main_layout),
      moos_scope_config_(moos_scope_config),
      hr_(new WText("<hr />", this)),
      add_text_(new WText(("Add history for key: "), this)),
      history_box_(new WComboBox(this)),
      history_button_(new WPushButton("Add", this))

{
    history_box_->setModel(model);
    history_button_->clicked().connect(this, &HistoryContainer::handle_add_history);
}

void goby::common::LiaisonScope::HistoryContainer::handle_add_history()
{
    std::string selected_key = history_box_->currentText().narrow();
    goby::common::protobuf::MOOSScopeConfig::HistoryConfig config;
    config.set_key(selected_key);
    add_history(config);
}

void goby::common::LiaisonScope::HistoryContainer::add_history(const goby::common::protobuf::MOOSScopeConfig::HistoryConfig& config)
{
    const std::string& selected_key = config.key();
    
    if(!history_models_.count(selected_key))
    {
        Wt::WContainerWidget* new_container = new WContainerWidget;

        Wt::WContainerWidget* text_container = new WContainerWidget(new_container);
        new WText("History for  ", text_container);
        WPushButton* remove_history_button = new WPushButton(selected_key, text_container);

        remove_history_button->clicked().connect(
            boost::bind(&HistoryContainer::handle_remove_history, this, selected_key));
        
        new WText(" (click to remove)", text_container);
        new WBreak(text_container);
        // WPushButton* toggle_plot_button = new WPushButton("Plot", text_container);

        
//        text_container->resize(Wt::WLength::Auto, WLength(4, WLength::FontEm));

        Wt::WStandardItemModel* new_model = new LiaisonScopeMOOSModel(moos_scope_config_,
                                                                      new_container);
        
        Wt::WSortFilterProxyModel* new_proxy = new Wt::WSortFilterProxyModel(new_container);
        new_proxy->setSourceModel(new_model);

        
        // Chart::WCartesianChart* chart = new Chart::WCartesianChart(new_container);
        // toggle_plot_button->clicked().connect(
        //     boost::bind(&HistoryContainer::toggle_history_plot, this, chart));
        // chart->setModel(new_model);    
        // chart->setXSeriesColumn(protobuf::MOOSScopeConfig::COLUMN_TIME); 
        // Chart::WDataSeries s(protobuf::MOOSScopeConfig::COLUMN_VALUE, Chart::LineSeries);
        // chart->addSeries(s);        
        
        // chart->setType(Chart::ScatterPlot);
        // chart->axis(Chart::XAxis).setScale(Chart::DateTimeScale); 
        // chart->axis(Chart::XAxis).setTitle("Time"); 
        // chart->axis(Chart::YAxis).setTitle(selected_key); 

        // WFont font;
        // font.setFamily(WFont::Serif, "Gentium");
        // chart->axis(Chart::XAxis).setTitleFont(font); 
        // chart->axis(Chart::YAxis).setTitleFont(font); 

        
        // // Provide space for the X and Y axis and title. 
        // chart->setPlotAreaPadding(80, Left);
        // chart->setPlotAreaPadding(40, Top | Bottom);
        // chart->setMargin(10, Top | Bottom);            // add margin vertically
        // chart->setMargin(WLength::Auto, Left | Right); // center horizontally
        // chart->resize(config.plot_width(), config.plot_height());

        // if(!config.show_plot())
        //     chart->hide();
        
        Wt::WTreeView* new_tree = new LiaisonScopeMOOSTreeView(moos_scope_config_, new_container);        
        main_layout_->insertWidget(main_layout_->count()-2, new_container);
        // set the widget *before* the one we just inserted to be resizable
        main_layout_->setResizable(main_layout_->count()-3);

        //main_layout_->insertWidget(main_layout_->count()-2, new_tree);
        // set the widget *before* the one we just inserted to be resizable
        //main_layout_->setResizable(main_layout_->count()-3);
        
        new_tree->setModel(new_proxy);
        MVC& mvc = history_models_[selected_key];
        mvc.key = selected_key;
        mvc.container = new_container;
        mvc.model = new_model;
        mvc.tree = new_tree;
        mvc.proxy = new_proxy;

        node_->zeromq_service()->socket_from_id(
            Liaison::LIAISON_INTERNAL_SCOPE_SUBSCRIBE_SOCKET).set_blackout(
            MARSHALLING_MOOS,
            "CMOOSMsg/" + selected_key + "/",
            boost::posix_time::milliseconds(0));

        new_proxy->setFilterRegExp(".*");
        new_tree->sortByColumn(protobuf::MOOSScopeConfig::COLUMN_TIME,
                               DescendingOrder);

    }
}

void goby::common::LiaisonScope::HistoryContainer::handle_remove_history(std::string key)
{
    glog.is(DEBUG2, lock) && glog << "LiaisonScope: removing history for: " << key << std::endl << unlock;

    
    main_layout_->removeWidget(history_models_[key].container);    
    main_layout_->removeWidget(history_models_[key].tree);

    delete history_models_[key].container;
    delete history_models_[key].tree;
    history_models_.erase(key);
}

void goby::common::LiaisonScope::HistoryContainer::toggle_history_plot(Wt::WWidget* plot)
{
    if(plot->isHidden())
        plot->show();
    else
        plot->hide();
}



goby::common::LiaisonScope::RegexFilterContainer::RegexFilterContainer(
    Wt::WStandardItemModel* model,
    Wt::WSortFilterProxyModel* proxy,
    const protobuf::MOOSScopeConfig& moos_scope_config,
    Wt::WContainerWidget* parent /* = 0 */)
    : Wt::WContainerWidget(parent),
      model_(model),
      proxy_(proxy),
      hr_(new WText("<hr />", this)),
      set_text_(new WText(("Set regex filter: Column: "), this)),
      regex_column_select_(new Wt::WComboBox(this)),
      expression_text_(new WText((" Expression: "), this)),
      regex_filter_text_(new WLineEdit(moos_scope_config.regex_filter_expression(), this)),
      regex_filter_button_(new WPushButton("Set", this)),
      regex_filter_clear_(new WPushButton("Clear", this)),
      regex_filter_examples_(new WPushButton("Examples", this)),
      break_(new WBreak(this)),
      regex_examples_table_(new WTable(this))
{     
    for(int i = 0, n = model_->columnCount(); i < n; ++i)
        regex_column_select_->addItem(boost::any_cast<std::string>(model_->headerData(i)));
    regex_column_select_->setCurrentIndex(moos_scope_config.regex_filter_column());
    
    regex_filter_button_->clicked().connect(this, &RegexFilterContainer::handle_set_regex_filter);
    regex_filter_clear_->clicked().connect(this, &RegexFilterContainer::handle_clear_regex_filter);
    regex_filter_text_->enterPressed().connect(this, &RegexFilterContainer::handle_set_regex_filter);
    regex_filter_examples_->clicked().connect(this, &RegexFilterContainer::toggle_regex_examples_table);

    regex_examples_table_->setHeaderCount(1);
    regex_examples_table_->setStyleClass("basic_small");
    new WText("Expression", regex_examples_table_->elementAt(0,0));
    new WText("Matches", regex_examples_table_->elementAt(0,1));
    new WText(".*", regex_examples_table_->elementAt(1,0));
    new WText("anything", regex_examples_table_->elementAt(1,1));
    new WText(".*_STATUS", regex_examples_table_->elementAt(2,0));
    new WText("fields ending in \"_STATUS\"",regex_examples_table_->elementAt(2,1));
    new WText(".*[^(_STATUS)]",regex_examples_table_->elementAt(3,0));
    new WText("fields <i>not</i> ending in \"_STATUS\"",regex_examples_table_->elementAt(3,1));
    new WText("-?[[:digit:]]*\\.[[:digit:]]*e?-?[[:digit:]]*",regex_examples_table_->elementAt(4,0));
    new WText("a floating point number (e.g. -3.456643e7)", regex_examples_table_->elementAt(4,1));
    regex_examples_table_->hide();

    handle_set_regex_filter();
}


void goby::common::LiaisonScope::RegexFilterContainer::handle_set_regex_filter()
{
    proxy_->setFilterKeyColumn(regex_column_select_->currentIndex());
    proxy_->setFilterRegExp(regex_filter_text_->text());
}


void goby::common::LiaisonScope::RegexFilterContainer::handle_clear_regex_filter()
{
    regex_filter_text_->setText(".*");
    handle_set_regex_filter();
}

void goby::common::LiaisonScope::RegexFilterContainer::toggle_regex_examples_table()
{
    regex_examples_table_->isHidden() ?
        regex_examples_table_->show() :
        regex_examples_table_->hide();
    
}
                          
                          
