#include "visual_script.h"
#include "visual_script_nodes.h"
#include "globals.h"
#define SCRIPT_VARIABLES_PREFIX "script_variables/"


//used by editor, this is not really saved
void VisualScriptNode::set_breakpoint(bool p_breakpoint) {
	breakpoint=p_breakpoint;
}

bool VisualScriptNode::is_breakpoint() const {

	return breakpoint;
}

void VisualScriptNode::_notification(int p_what) {

	if (p_what==NOTIFICATION_POSTINITIALIZE) {

		int dvc = get_input_value_port_count();
		for(int i=0;i<dvc;i++) {
			Variant::Type expected = get_input_value_port_info(i).type;
			Variant::CallError ce;
			default_input_values.push_back(Variant::construct(expected,NULL,0,ce,false));
		}
	}
}

void VisualScriptNode::ports_changed_notify(){

	default_input_values.resize( MAX(default_input_values.size(),get_input_value_port_count()) ); //let it grow as big as possible, we don't want to lose values on resize
	emit_signal("ports_changed");
}

void  VisualScriptNode::set_default_input_value(int p_port,const Variant& p_value) {

	ERR_FAIL_INDEX(p_port,default_input_values.size());

	default_input_values[p_port]=p_value;
}

Variant VisualScriptNode::get_default_input_value(int p_port) const {

	ERR_FAIL_INDEX_V(p_port,default_input_values.size(),Variant());
	return default_input_values[p_port];
}

void VisualScriptNode::_set_default_input_values(Array p_values) {


	default_input_values=p_values;
}

Array VisualScriptNode::_get_default_input_values() const {

	//validate on save, since on load there is little info about this

	Array saved_values;

	//actually validate on save
	for(int i=0;i<get_input_value_port_count();i++) {

		Variant::Type expected = get_input_value_port_info(i).type;

		if (i>=default_input_values.size()) {

			Variant::CallError ce;
			saved_values.push_back(Variant::construct(expected,NULL,0,ce,false));
		} else {

			if (expected==Variant::NIL || expected==default_input_values[i].get_type()) {
				saved_values.push_back(default_input_values[i]);
			} else  {
				//not the same, reconvert
				Variant::CallError ce;
				Variant existing = default_input_values[i];
				const Variant *existingp=&existing;
				saved_values.push_back( Variant::construct(expected,&existingp,1,ce,false) );
			}
		}
	}
	return saved_values;
}



void VisualScriptNode::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("get_visual_script:VisualScript"),&VisualScriptNode::get_visual_script);
	ObjectTypeDB::bind_method(_MD("set_default_input_value","port_idx","value:Variant"),&VisualScriptNode::set_default_input_value);
	ObjectTypeDB::bind_method(_MD("get_default_input_value:Variant","port_idx"),&VisualScriptNode::get_default_input_value);
	ObjectTypeDB::bind_method(_MD("_set_default_input_values","values"),&VisualScriptNode::_set_default_input_values);
	ObjectTypeDB::bind_method(_MD("_get_default_input_values"),&VisualScriptNode::_get_default_input_values);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY,"_default_input_values",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NOEDITOR),_SCS("_set_default_input_values"),_SCS("_get_default_input_values"));
	ADD_SIGNAL(MethodInfo("ports_changed"));
}


Ref<VisualScript> VisualScriptNode::get_visual_script() const {

	if (scripts_used.size())
		return Ref<VisualScript>(scripts_used.front()->get());

	return Ref<VisualScript>();

}

VisualScriptNode::VisualScriptNode() {
	breakpoint=false;
}

////////////////

/////////////////////

VisualScriptNodeInstance::VisualScriptNodeInstance() {

	sequence_outputs=NULL;
	input_ports=NULL;
}

VisualScriptNodeInstance::~VisualScriptNodeInstance() {

	if (sequence_outputs) {
		memdelete(sequence_outputs);
	}

	if (input_ports) {
		memdelete(input_ports);
	}

	if (output_ports) {
		memdelete(output_ports);
	}

}

void VisualScript::add_function(const StringName& p_name) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!String(p_name).is_valid_identifier());
	ERR_FAIL_COND(functions.has(p_name));

	functions[p_name]=Function();
	functions[p_name].scroll=Vector2(-50,-100);
}

bool VisualScript::has_function(const StringName& p_name) const {

	return functions.has(p_name);

}
void VisualScript::remove_function(const StringName& p_name) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!functions.has(p_name));

	for (Map<int,Function::NodeData>::Element *E=functions[p_name].nodes.front();E;E=E->next()) {

		E->get().node->disconnect("ports_changed",this,"_node_ports_changed");
		E->get().node->scripts_used.erase(this);
	}

	functions.erase(p_name);

}

void VisualScript::rename_function(const StringName& p_name,const StringName& p_new_name) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!functions.has(p_name));
	if (p_new_name==p_name)
		return;

	ERR_FAIL_COND(!String(p_new_name).is_valid_identifier());

	ERR_FAIL_COND(functions.has(p_new_name));
	ERR_FAIL_COND(variables.has(p_new_name));
	ERR_FAIL_COND(custom_signals.has(p_new_name));

	functions[p_new_name]=functions[p_name];
	functions.erase(p_name);

}

void VisualScript::set_function_scroll(const StringName& p_name, const Vector2& p_scroll) {

	ERR_FAIL_COND(!functions.has(p_name));
	functions[p_name].scroll=p_scroll;

}

Vector2 VisualScript::get_function_scroll(const StringName& p_name) const {

	ERR_FAIL_COND_V(!functions.has(p_name),Vector2());
	return functions[p_name].scroll;

}


void VisualScript::get_function_list(List<StringName> *r_functions) const {

	for (const Map<StringName,Function>::Element *E=functions.front();E;E=E->next()) {
		r_functions->push_back(E->key());
	}

	r_functions->sort_custom<StringName::AlphCompare>();

}

int VisualScript::get_function_node_id(const StringName& p_name) const {

	ERR_FAIL_COND_V(!functions.has(p_name),-1);

	return functions[p_name].function_id;

}


void VisualScript::_node_ports_changed(int p_id) {


	StringName function;

	for (Map<StringName,Function>::Element *E=functions.front();E;E=E->next()) {

		if (E->get().nodes.has(p_id)) {
			function=E->key();
			break;
		}
	}

	ERR_FAIL_COND(function==StringName());

	Function &func = functions[function];
	Ref<VisualScriptNode> vsn = func.nodes[p_id].node;

	//must revalidate all the functions

	{
		List<SequenceConnection> to_remove;

		for (Set<SequenceConnection>::Element *E=func.sequence_connections.front();E;E=E->next()) {
			if (E->get().from_node==p_id && E->get().from_output>=vsn->get_output_sequence_port_count()) {

				to_remove.push_back(E->get());
			}
			if (E->get().to_node==p_id && !vsn->has_input_sequence_port()) {

				to_remove.push_back(E->get());
			}
		}

		while(to_remove.size()) {
			func.sequence_connections.erase(to_remove.front()->get());
			to_remove.pop_front();
		}
	}

	{

		List<DataConnection> to_remove;


		for (Set<DataConnection>::Element *E=func.data_connections.front();E;E=E->next()) {
			if (E->get().from_node==p_id && E->get().from_port>=vsn->get_output_value_port_count()) {
				to_remove.push_back(E->get());
			}
			if (E->get().to_node==p_id && E->get().to_port>=vsn->get_input_value_port_count()) {
				to_remove.push_back(E->get());
			}
		}

		while(to_remove.size()) {
			func.data_connections.erase(to_remove.front()->get());
			to_remove.pop_front();
		}
	}

	emit_signal("node_ports_changed",function,p_id);
}

void VisualScript::add_node(const StringName& p_func,int p_id, const Ref<VisualScriptNode>& p_node, const Point2 &p_pos) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!functions.has(p_func));


	for (Map<StringName,Function>::Element *E=functions.front();E;E=E->next()) {

		ERR_FAIL_COND(E->get().nodes.has(p_id)); //id can exist only one in script, even for different functions
	}

	Function &func = functions[p_func];


	if (p_node->cast_to<VisualScriptFunction>()) {
		//the function indeed
		ERR_EXPLAIN("A function node already has been set here.");
		ERR_FAIL_COND(func.function_id>=0);

		func.function_id=p_id;
	}

	Function::NodeData nd;
	nd.node=p_node;
	nd.pos=p_pos;

	Ref<VisualScriptNode> vsn = p_node;
	vsn->connect("ports_changed",this,"_node_ports_changed",varray(p_id));
	vsn->scripts_used.insert(this);



	func.nodes[p_id]=nd;
}

void VisualScript::remove_node(const StringName& p_func,int p_id){

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!functions.has(p_func));
	Function &func = functions[p_func];

	ERR_FAIL_COND(!func.nodes.has(p_id));
	{
		List<SequenceConnection> to_remove;

		for (Set<SequenceConnection>::Element *E=func.sequence_connections.front();E;E=E->next()) {
			if (E->get().from_node==p_id || E->get().to_node==p_id) {
				to_remove.push_back(E->get());
			}
		}

		while(to_remove.size()) {
			func.sequence_connections.erase(to_remove.front()->get());
			to_remove.pop_front();
		}
	}

	{

		List<DataConnection> to_remove;


		for (Set<DataConnection>::Element *E=func.data_connections.front();E;E=E->next()) {
			if (E->get().from_node==p_id || E->get().to_node==p_id) {
				to_remove.push_back(E->get());
			}
		}

		while(to_remove.size()) {
			func.data_connections.erase(to_remove.front()->get());
			to_remove.pop_front();
		}
	}

	if (func.nodes[p_id].node->cast_to<VisualScriptFunction>()) {
		func.function_id=-1; //revert to invalid
	}

	func.nodes[p_id].node->disconnect("ports_changed",this,"_node_ports_changed");
	func.nodes[p_id].node->scripts_used.erase(this);

	func.nodes.erase(p_id);


}


bool VisualScript::has_node(const StringName& p_func,int p_id) const {

	ERR_FAIL_COND_V(!functions.has(p_func),false);
	const Function &func = functions[p_func];

	return func.nodes.has(p_id);
}

Ref<VisualScriptNode> VisualScript::get_node(const StringName& p_func,int p_id) const{

	ERR_FAIL_COND_V(!functions.has(p_func),Ref<VisualScriptNode>());
	const Function &func = functions[p_func];

	ERR_FAIL_COND_V(!func.nodes.has(p_id),Ref<VisualScriptNode>());

	return func.nodes[p_id].node;
}

void VisualScript::set_node_pos(const StringName& p_func,int p_id,const Point2& p_pos) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!functions.has(p_func));
	Function &func = functions[p_func];

	ERR_FAIL_COND(!func.nodes.has(p_id));
	func.nodes[p_id].pos=p_pos;
}

Point2 VisualScript::get_node_pos(const StringName& p_func,int p_id) const{

	ERR_FAIL_COND_V(!functions.has(p_func),Point2());
	const Function &func = functions[p_func];

	ERR_FAIL_COND_V(!func.nodes.has(p_id),Point2());
	return func.nodes[p_id].pos;
}


void VisualScript::get_node_list(const StringName& p_func,List<int> *r_nodes) const{

	ERR_FAIL_COND(!functions.has(p_func));
	const Function &func = functions[p_func];

	for (const Map<int,Function::NodeData>::Element *E=func.nodes.front();E;E=E->next()) {
		r_nodes->push_back(E->key());
	}

}


void VisualScript::sequence_connect(const StringName& p_func,int p_from_node,int p_from_output,int p_to_node){

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!functions.has(p_func));
	Function &func = functions[p_func];


	SequenceConnection sc;
	sc.from_node=p_from_node;
	sc.from_output=p_from_output;
	sc.to_node=p_to_node;
	ERR_FAIL_COND(func.sequence_connections.has(sc));

	func.sequence_connections.insert(sc);

}

void VisualScript::sequence_disconnect(const StringName& p_func,int p_from_node,int p_from_output,int p_to_node){

	ERR_FAIL_COND(!functions.has(p_func));
	Function &func = functions[p_func];

	SequenceConnection sc;
	sc.from_node=p_from_node;
	sc.from_output=p_from_output;
	sc.to_node=p_to_node;
	ERR_FAIL_COND(!func.sequence_connections.has(sc));

	func.sequence_connections.erase(sc);

}

bool VisualScript::has_sequence_connection(const StringName& p_func,int p_from_node,int p_from_output,int p_to_node) const{

	ERR_FAIL_COND_V(!functions.has(p_func),false);
	const Function &func = functions[p_func];

	SequenceConnection sc;
	sc.from_node=p_from_node;
	sc.from_output=p_from_output;
	sc.to_node=p_to_node;

	return func.sequence_connections.has(sc);
}

void VisualScript::get_sequence_connection_list(const StringName& p_func,List<SequenceConnection> *r_connection) const {

	ERR_FAIL_COND(!functions.has(p_func));
	const Function &func = functions[p_func];

	for (const Set<SequenceConnection>::Element *E=func.sequence_connections.front();E;E=E->next()) {
		r_connection->push_back(E->get());
	}
}


void VisualScript::data_connect(const StringName& p_func,int p_from_node,int p_from_port,int p_to_node,int p_to_port) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!functions.has(p_func));
	Function &func = functions[p_func];

	DataConnection dc;
	dc.from_node=p_from_node;
	dc.from_port=p_from_port;
	dc.to_node=p_to_node;
	dc.to_port=p_to_port;

	ERR_FAIL_COND( func.data_connections.has(dc));

	func.data_connections.insert(dc);
}

void VisualScript::data_disconnect(const StringName& p_func,int p_from_node,int p_from_port,int p_to_node,int p_to_port) {

	ERR_FAIL_COND(!functions.has(p_func));
	Function &func = functions[p_func];

	DataConnection dc;
	dc.from_node=p_from_node;
	dc.from_port=p_from_port;
	dc.to_node=p_to_node;
	dc.to_port=p_to_port;

	ERR_FAIL_COND( !func.data_connections.has(dc));

	func.data_connections.erase(dc);

}

bool VisualScript::has_data_connection(const StringName& p_func,int p_from_node,int p_from_port,int p_to_node,int p_to_port) const {

	ERR_FAIL_COND_V(!functions.has(p_func),false);
	const Function &func = functions[p_func];

	DataConnection dc;
	dc.from_node=p_from_node;
	dc.from_port=p_from_port;
	dc.to_node=p_to_node;
	dc.to_port=p_to_port;

	return func.data_connections.has(dc);

}

bool VisualScript::is_input_value_port_connected(const StringName& p_func,int p_node,int p_port) const {

	ERR_FAIL_COND_V(!functions.has(p_func),false);
	const Function &func = functions[p_func];

	for (const Set<DataConnection>::Element *E=func.data_connections.front();E;E=E->next()) {
		if (E->get().to_node==p_node && E->get().to_port==p_port)
			return true;
	}

	return false;
}

void VisualScript::get_data_connection_list(const StringName& p_func,List<DataConnection> *r_connection) const {

	ERR_FAIL_COND(!functions.has(p_func));
	const Function &func = functions[p_func];

	for (const Set<DataConnection>::Element *E=func.data_connections.front();E;E=E->next()) {
		r_connection->push_back(E->get());
	}
}

void VisualScript::add_variable(const StringName& p_name,const Variant& p_default_value) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!String(p_name).is_valid_identifier());
	ERR_FAIL_COND(variables.has(p_name));

	Variable v;
	v.default_value=p_default_value;
	v.info.type=p_default_value.get_type();
	v.info.name=p_name;
	v.info.hint=PROPERTY_HINT_NONE;

	variables[p_name]=v;
	script_variable_remap[SCRIPT_VARIABLES_PREFIX+String(p_name)]=p_name;


#ifdef TOOLS_ENABLED
	_update_placeholders();
#endif

}

bool VisualScript::has_variable(const StringName& p_name) const {

	return variables.has(p_name);
}

void VisualScript::remove_variable(const StringName& p_name) {

	ERR_FAIL_COND(!variables.has(p_name));
	variables.erase(p_name);
	script_variable_remap.erase(SCRIPT_VARIABLES_PREFIX+String(p_name));

#ifdef TOOLS_ENABLED
	_update_placeholders();
#endif
}

void VisualScript::set_variable_default_value(const StringName& p_name,const Variant& p_value){

	ERR_FAIL_COND(!variables.has(p_name));

	variables[p_name].default_value=p_value;

#ifdef TOOLS_ENABLED
	_update_placeholders();
#endif

}
Variant VisualScript::get_variable_default_value(const StringName& p_name) const{

	ERR_FAIL_COND_V(!variables.has(p_name),Variant());
	return variables[p_name].default_value;

}
void VisualScript::set_variable_info(const StringName& p_name,const PropertyInfo& p_info){

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!variables.has(p_name));
	variables[p_name].info=p_info;
	variables[p_name].info.name=p_name;

#ifdef TOOLS_ENABLED
	_update_placeholders();
#endif


}
PropertyInfo VisualScript::get_variable_info(const StringName& p_name) const{

	ERR_FAIL_COND_V(!variables.has(p_name),PropertyInfo());
	return variables[p_name].info;
}

void VisualScript::_set_variable_info(const StringName& p_name,const Dictionary& p_info) {

	PropertyInfo pinfo;
	if (p_info.has("type"))
		pinfo.type=Variant::Type(int(p_info["type"]));
	if (p_info.has("name"))
		pinfo.name=p_info["name"];
	if (p_info.has("hint"))
		pinfo.hint=PropertyHint(int(p_info["hint"]));
	if (p_info.has("hint_string"))
		pinfo.hint_string=p_info["hint_string"];
	if (p_info.has("usage"))
		pinfo.usage=p_info["usage"];

	set_variable_info(p_name,pinfo);
}

Dictionary VisualScript::_get_variable_info(const StringName& p_name) const{

	PropertyInfo pinfo=get_variable_info(p_name);
	Dictionary d;
	d["type"]=pinfo.type;
	d["name"]=pinfo.name;
	d["hint"]=pinfo.hint;
	d["hint_string"]=pinfo.hint_string;
	d["usage"]=pinfo.usage;

	return d;
}

void VisualScript::get_variable_list(List<StringName> *r_variables){


	for (Map<StringName,Variable>::Element *E=variables.front();E;E=E->next()) {
		r_variables->push_back(E->key());
	}

	r_variables->sort_custom<StringName::AlphCompare>();
}


void VisualScript::set_instance_base_type(const StringName& p_type) {

	ERR_FAIL_COND( instances.size() );
	base_type=p_type;
}


void VisualScript::rename_variable(const StringName& p_name,const StringName& p_new_name) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!variables.has(p_name));
	if (p_new_name==p_name)
		return;

	ERR_FAIL_COND(!String(p_new_name).is_valid_identifier());

	ERR_FAIL_COND(functions.has(p_new_name));
	ERR_FAIL_COND(variables.has(p_new_name));
	ERR_FAIL_COND(custom_signals.has(p_new_name));

	variables[p_new_name]=variables[p_name];
	variables.erase(p_name);

}

void VisualScript::add_custom_signal(const StringName& p_name) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!String(p_name).is_valid_identifier());
	ERR_FAIL_COND(custom_signals.has(p_name));

	custom_signals[p_name]=Vector<Argument>();
}

bool VisualScript::has_custom_signal(const StringName& p_name) const {

	return custom_signals.has(p_name);

}
void VisualScript::custom_signal_add_argument(const StringName& p_func,Variant::Type p_type,const String& p_name,int p_index) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!custom_signals.has(p_func));
	Argument arg;
	arg.type=p_type;
	arg.name=p_name;
	if (p_index<0)
		custom_signals[p_func].push_back(arg);
	else
		custom_signals[p_func].insert(0,arg);

}
void VisualScript::custom_signal_set_argument_type(const StringName& p_func,int p_argidx,Variant::Type p_type) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!custom_signals.has(p_func));
	ERR_FAIL_INDEX(p_argidx,custom_signals[p_func].size());
	custom_signals[p_func][p_argidx].type=p_type;
}
Variant::Type VisualScript::custom_signal_get_argument_type(const StringName& p_func,int p_argidx) const  {

	ERR_FAIL_COND_V(!custom_signals.has(p_func),Variant::NIL);
	ERR_FAIL_INDEX_V(p_argidx,custom_signals[p_func].size(),Variant::NIL);
	return custom_signals[p_func][p_argidx].type;
}
void VisualScript::custom_signal_set_argument_name(const StringName& p_func,int p_argidx,const String& p_name) {
	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!custom_signals.has(p_func));
	ERR_FAIL_INDEX(p_argidx,custom_signals[p_func].size());
	custom_signals[p_func][p_argidx].name=p_name;

}
String VisualScript::custom_signal_get_argument_name(const StringName& p_func,int p_argidx) const {

	ERR_FAIL_COND_V(!custom_signals.has(p_func),String());
	ERR_FAIL_INDEX_V(p_argidx,custom_signals[p_func].size(),String());
	return custom_signals[p_func][p_argidx].name;

}
void VisualScript::custom_signal_remove_argument(const StringName& p_func,int p_argidx) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!custom_signals.has(p_func));
	ERR_FAIL_INDEX(p_argidx,custom_signals[p_func].size());
	custom_signals[p_func].remove(p_argidx);

}

int VisualScript::custom_signal_get_argument_count(const StringName& p_func) const {

	ERR_FAIL_COND_V(!custom_signals.has(p_func),0);
	return custom_signals[p_func].size();

}
void VisualScript::custom_signal_swap_argument(const StringName& p_func,int p_argidx,int p_with_argidx) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!custom_signals.has(p_func));
	ERR_FAIL_INDEX(p_argidx,custom_signals[p_func].size());
	ERR_FAIL_INDEX(p_with_argidx,custom_signals[p_func].size());

	SWAP( custom_signals[p_func][p_argidx], custom_signals[p_func][p_with_argidx] );

}
void VisualScript::remove_custom_signal(const StringName& p_name) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!custom_signals.has(p_name));
	custom_signals.erase(p_name);

}

void VisualScript::rename_custom_signal(const StringName& p_name,const StringName& p_new_name) {

	ERR_FAIL_COND( instances.size() );
	ERR_FAIL_COND(!custom_signals.has(p_name));
	if (p_new_name==p_name)
		return;

	ERR_FAIL_COND(!String(p_new_name).is_valid_identifier());

	ERR_FAIL_COND(functions.has(p_new_name));
	ERR_FAIL_COND(variables.has(p_new_name));
	ERR_FAIL_COND(custom_signals.has(p_new_name));

	custom_signals[p_new_name]=custom_signals[p_name];
	custom_signals.erase(p_name);

}

void VisualScript::get_custom_signal_list(List<StringName> *r_custom_signals) const {

	for (const Map<StringName,Vector<Argument> >::Element *E=custom_signals.front();E;E=E->next()) {
		r_custom_signals->push_back(E->key());
	}

	r_custom_signals->sort_custom<StringName::AlphCompare>();

}

int VisualScript::get_available_id() const {

	int max_id=0;
	for (Map<StringName,Function>::Element *E=functions.front();E;E=E->next()) {
		if (E->get().nodes.empty())
			continue;

		int last_id = E->get().nodes.back()->key();
		max_id=MAX(max_id,last_id+1);
	}

	return max_id;
}

/////////////////////////////////


bool VisualScript::can_instance() const {

	return true;//ScriptServer::is_scripting_enabled();

}


StringName VisualScript::get_instance_base_type() const {

	return base_type;
}


#ifdef TOOLS_ENABLED
void VisualScript::_placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {


	placeholders.erase(p_placeholder);
}


void VisualScript::_update_placeholders() {

	if (placeholders.size()==0)
		return; //no bother if no placeholders
	List<PropertyInfo> pinfo;
	Map<StringName,Variant> values;

	for (Map<StringName,Variable>::Element *E=variables.front();E;E=E->next()) {

		PropertyInfo p = E->get().info;
		p.name=SCRIPT_VARIABLES_PREFIX+String(E->key());
		pinfo.push_back(p);
		values[p.name]=E->get().default_value;
	}

	for (Set<PlaceHolderScriptInstance*>::Element *E=placeholders.front();E;E=E->next()) {

		E->get()->update(pinfo,values);
	}

}

#endif


ScriptInstance* VisualScript::instance_create(Object *p_this) {



#ifdef TOOLS_ENABLED

	if (!ScriptServer::is_scripting_enabled()) {


		PlaceHolderScriptInstance *sins = memnew( PlaceHolderScriptInstance(VisualScriptLanguage::singleton,Ref<Script>((Script*)this),p_this));
		placeholders.insert(sins);

		List<PropertyInfo> pinfo;
		Map<StringName,Variant> values;

		for (Map<StringName,Variable>::Element *E=variables.front();E;E=E->next()) {

			PropertyInfo p = E->get().info;
			p.name=SCRIPT_VARIABLES_PREFIX+String(E->key());
			pinfo.push_back(p);
			values[p.name]=E->get().default_value;
		}

		sins->update(pinfo,values);

		return sins;
	}
#endif


	VisualScriptInstance *instance=memnew(  VisualScriptInstance );
	instance->create(Ref<VisualScript>(this),p_this);


	if (VisualScriptLanguage::singleton->lock)
		VisualScriptLanguage::singleton->lock->lock();

	instances[p_this]=instance;

	if (VisualScriptLanguage::singleton->lock)
		VisualScriptLanguage::singleton->lock->unlock();

	return instance;
}

bool VisualScript::instance_has(const Object *p_this) const {

	return instances.has((Object*)p_this);
}

bool VisualScript::has_source_code() const {

	return false;
}

String VisualScript::get_source_code() const {

	return String();
}

void VisualScript::set_source_code(const String& p_code) {

}

Error VisualScript::reload(bool p_keep_state) {

	return OK;
}


bool VisualScript::is_tool() const {

	return false;
}


String VisualScript::get_node_type() const {

	return String();
}


ScriptLanguage *VisualScript::get_language() const {

	return VisualScriptLanguage::singleton;
}


bool VisualScript::has_script_signal(const StringName& p_signal) const {

	return custom_signals.has(p_signal);
}

void VisualScript::get_script_signal_list(List<MethodInfo> *r_signals) const {

	for (const Map<StringName,Vector<Argument> >::Element *E=custom_signals.front();E;E=E->next()) {

		MethodInfo mi;
		mi.name=E->key();
		for(int i=0;i<E->get().size();i++) {
			PropertyInfo arg;
			arg.type=E->get()[i].type;
			arg.name=E->get()[i].name;
			mi.arguments.push_back(arg);
		}


		r_signals->push_back(mi);
	}


}


bool VisualScript::get_property_default_value(const StringName& p_property,Variant& r_value) const {

	if (!script_variable_remap.has(p_property))
		return false;

	r_value=variables[ script_variable_remap[p_property] ].default_value;
	return true;
}
void VisualScript::get_method_list(List<MethodInfo> *p_list) const {

	for (Map<StringName,Function>::Element *E=functions.front();E;E=E->next()) {

		MethodInfo mi;
		mi.name=E->key();
		if (E->get().function_id>=0) {

			Ref<VisualScriptFunction> func=E->get().nodes[E->get().function_id].node;
			if (func.is_valid()) {

				for(int i=0;i<func->get_argument_count();i++) {
					PropertyInfo arg;
					arg.name=func->get_argument_name(i);
					arg.type=func->get_argument_type(i);
					mi.arguments.push_back(arg);
				}
			}
		}

		p_list->push_back(mi);
	}
}

void VisualScript::_set_data(const Dictionary& p_data) {

	Dictionary d = p_data;
	if (d.has("base_type"))
		base_type=d["base_type"];

	variables.clear();
	Array vars=d["variables"];
	for (int i=0;i<vars.size();i++) {

		Dictionary v=vars[i];
		StringName name = v["name"];
		add_variable(name);
		_set_variable_info(name,v);
		set_variable_default_value(name,v["default_value"]);

	}


	custom_signals.clear();
	Array sigs=d["signals"];
	for (int i=0;i<sigs.size();i++) {

		Dictionary cs=sigs[i];
		add_custom_signal(cs["name"]);

		Array args=cs["arguments"];
		for(int j=0;j<args.size();j+=2) {
			custom_signal_add_argument(cs["name"],Variant::Type(int(args[j+1])),args[j]);
		}
	}

	Array funcs=d["functions"];
	functions.clear();

	for (int i=0;i<funcs.size();i++) {

		Dictionary func=funcs[i];


		StringName name=func["name"];
		//int id=func["function_id"];
		add_function(name);

		set_function_scroll(name,func["scroll"]);

		Array nodes = func["nodes"];

		for(int i=0;i<nodes.size();i+=3) {

			add_node(name,nodes[i],nodes[i+2],nodes[i+1]);
		}


		Array sequence_connections=func["sequence_connections"];

		for (int j=0;j<sequence_connections.size();j+=3) {

			sequence_connect(name,sequence_connections[j+0],sequence_connections[j+1],sequence_connections[j+2]);
		}


		Array data_connections=func["data_connections"];

		for (int j=0;j<data_connections.size();j+=4) {

			data_connect(name,data_connections[j+0],data_connections[j+1],data_connections[j+2],data_connections[j+3]);

		}


	}

}

Dictionary VisualScript::_get_data() const{

	Dictionary d;
	d["base_type"]=base_type;
	Array vars;
	for (const Map<StringName,Variable>::Element *E=variables.front();E;E=E->next()) {

		Dictionary var = _get_variable_info(E->key());
		var["name"]=E->key(); //make sure it's the right one
		var["default_value"]=E->get().default_value;
		vars.push_back(var);
	}
	d["variables"]=vars;

	Array sigs;
	for (const Map<StringName,Vector<Argument> >::Element *E=custom_signals.front();E;E=E->next()) {

		Dictionary cs;
		cs["name"]=E->key();
		Array args;
		for(int i=0;i<E->get().size();i++) {
			args.push_back(E->get()[i].name);
			args.push_back(E->get()[i].type);
		}
		cs["arguments"]=args;

		sigs.push_back(cs);
	}

	d["signals"]=sigs;

	Array funcs;

	for (const Map<StringName,Function>::Element *E=functions.front();E;E=E->next()) {

		Dictionary func;
		func["name"]=E->key();
		func["function_id"]=E->get().function_id;
		func["scroll"]=E->get().scroll;

		Array nodes;

		for (const Map<int,Function::NodeData>::Element *F=E->get().nodes.front();F;F=F->next()) {

			nodes.push_back(F->key());
			nodes.push_back(F->get().pos);
			nodes.push_back(F->get().node);

		}

		func["nodes"]=nodes;

		Array sequence_connections;

		for (const Set<SequenceConnection>::Element *F=E->get().sequence_connections.front();F;F=F->next()) {

			sequence_connections.push_back(F->get().from_node);
			sequence_connections.push_back(F->get().from_output);
			sequence_connections.push_back(F->get().to_node);

		}


		func["sequence_connections"]=sequence_connections;

		Array data_connections;

		for (const Set<DataConnection>::Element *F=E->get().data_connections.front();F;F=F->next()) {

			data_connections.push_back(F->get().from_node);
			data_connections.push_back(F->get().from_port);
			data_connections.push_back(F->get().to_node);
			data_connections.push_back(F->get().to_port);

		}


		func["data_connections"]=data_connections;

		funcs.push_back(func);

	}

	d["functions"]=funcs;


	return d;

}

void VisualScript::_bind_methods() {



	ObjectTypeDB::bind_method(_MD("_node_ports_changed"),&VisualScript::_node_ports_changed);

	ObjectTypeDB::bind_method(_MD("add_function","name"),&VisualScript::add_function);
	ObjectTypeDB::bind_method(_MD("has_function","name"),&VisualScript::has_function);	
	ObjectTypeDB::bind_method(_MD("remove_function","name"),&VisualScript::remove_function);
	ObjectTypeDB::bind_method(_MD("rename_function","name","new_name"),&VisualScript::rename_function);
	ObjectTypeDB::bind_method(_MD("set_function_scroll","ofs"),&VisualScript::set_function_scroll);
	ObjectTypeDB::bind_method(_MD("get_function_scroll"),&VisualScript::get_function_scroll);

	ObjectTypeDB::bind_method(_MD("add_node","func","id","node","pos"),&VisualScript::add_node,DEFVAL(Point2()));
	ObjectTypeDB::bind_method(_MD("remove_node","func","id"),&VisualScript::remove_node);
	ObjectTypeDB::bind_method(_MD("get_function_node_id","name"),&VisualScript::get_function_node_id);

	ObjectTypeDB::bind_method(_MD("get_node","func","id"),&VisualScript::get_node);
	ObjectTypeDB::bind_method(_MD("has_node","func","id"),&VisualScript::has_node);
	ObjectTypeDB::bind_method(_MD("set_node_pos","func","id","pos"),&VisualScript::set_node_pos);
	ObjectTypeDB::bind_method(_MD("get_node_pos","func","id"),&VisualScript::get_node_pos);

	ObjectTypeDB::bind_method(_MD("sequence_connect","func","from_node","from_output","to_node"),&VisualScript::sequence_connect);
	ObjectTypeDB::bind_method(_MD("sequence_disconnect","func","from_node","from_output","to_node"),&VisualScript::sequence_disconnect);
	ObjectTypeDB::bind_method(_MD("has_sequence_connection","func","from_node","from_output","to_node"),&VisualScript::has_sequence_connection);

	ObjectTypeDB::bind_method(_MD("data_connect","func","from_node","from_port","to_node","to_port"),&VisualScript::data_connect);
	ObjectTypeDB::bind_method(_MD("data_disconnect","func","from_node","from_port","to_node","to_port"),&VisualScript::data_disconnect);
	ObjectTypeDB::bind_method(_MD("has_data_connection","func","from_node","from_port","to_node","to_port"),&VisualScript::has_data_connection);

	ObjectTypeDB::bind_method(_MD("add_variable","name","default_value"),&VisualScript::add_variable,DEFVAL(Variant()));
	ObjectTypeDB::bind_method(_MD("has_variable","name"),&VisualScript::has_variable);
	ObjectTypeDB::bind_method(_MD("remove_variable","name"),&VisualScript::remove_variable);
	ObjectTypeDB::bind_method(_MD("set_variable_default_value","name","value"),&VisualScript::set_variable_default_value);
	ObjectTypeDB::bind_method(_MD("get_variable_default_value","name"),&VisualScript::get_variable_default_value);
	ObjectTypeDB::bind_method(_MD("set_variable_info","name","value"),&VisualScript::_set_variable_info);
	ObjectTypeDB::bind_method(_MD("get_variable_info","name"),&VisualScript::_get_variable_info);
	ObjectTypeDB::bind_method(_MD("rename_variable","name","new_name"),&VisualScript::rename_variable);

	ObjectTypeDB::bind_method(_MD("add_custom_signal","name"),&VisualScript::add_custom_signal);
	ObjectTypeDB::bind_method(_MD("has_custom_signal","name"),&VisualScript::has_custom_signal);
	ObjectTypeDB::bind_method(_MD("custom_signal_add_argument","name","type","argname","index"),&VisualScript::custom_signal_add_argument,DEFVAL(-1));
	ObjectTypeDB::bind_method(_MD("custom_signal_set_argument_type","name","argidx","type"),&VisualScript::custom_signal_set_argument_type);
	ObjectTypeDB::bind_method(_MD("custom_signal_get_argument_type","name","argidx"),&VisualScript::custom_signal_get_argument_type);
	ObjectTypeDB::bind_method(_MD("custom_signal_set_argument_name","name","argidx","argname"),&VisualScript::custom_signal_set_argument_name);
	ObjectTypeDB::bind_method(_MD("custom_signal_get_argument_name","name","argidx"),&VisualScript::custom_signal_get_argument_name);
	ObjectTypeDB::bind_method(_MD("custom_signal_remove_argument","argidx"),&VisualScript::custom_signal_remove_argument);
	ObjectTypeDB::bind_method(_MD("custom_signal_get_argument_count","name"),&VisualScript::custom_signal_get_argument_count);
	ObjectTypeDB::bind_method(_MD("custom_signal_swap_argument","name","argidx","withidx"),&VisualScript::custom_signal_swap_argument);
	ObjectTypeDB::bind_method(_MD("remove_custom_signal","name"),&VisualScript::remove_custom_signal);
	ObjectTypeDB::bind_method(_MD("rename_custom_signal","name","new_name"),&VisualScript::rename_custom_signal);

	//ObjectTypeDB::bind_method(_MD("set_variable_info","name","info"),&VScript::set_variable_info);
	//ObjectTypeDB::bind_method(_MD("get_variable_info","name"),&VScript::set_variable_info);

	ObjectTypeDB::bind_method(_MD("set_instance_base_type","type"),&VisualScript::set_instance_base_type);

	ObjectTypeDB::bind_method(_MD("_set_data","data"),&VisualScript::_set_data);
	ObjectTypeDB::bind_method(_MD("_get_data"),&VisualScript::_get_data);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY,"data",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NOEDITOR),_SCS("_set_data"),_SCS("_get_data"));

	ADD_SIGNAL(MethodInfo("node_ports_changed",PropertyInfo(Variant::STRING,"function"),PropertyInfo(Variant::INT,"id")));
}

VisualScript::VisualScript() {

	base_type="Object";

}

VisualScript::~VisualScript() {

	while(!functions.empty()) {
		remove_function(functions.front()->key());
	}

}

////////////////////////////////////////////



bool VisualScriptInstance::set(const StringName& p_name, const Variant& p_value) {

	const Map<StringName,StringName>::Element *remap = script->script_variable_remap.find(p_name);
	if (!remap)
		return false;

	Map<StringName,Variant>::Element *E=variables.find(remap->get());
	ERR_FAIL_COND_V(!E,false);

	E->get()=p_value;

	return true;
}


bool VisualScriptInstance::get(const StringName& p_name, Variant &r_ret) const {

	const Map<StringName,StringName>::Element *remap = script->script_variable_remap.find(p_name);
	if (!remap)
		return false;

	const Map<StringName,Variant>::Element *E=variables.find(remap->get());
	ERR_FAIL_COND_V(!E,false);

	return E->get();
}
void VisualScriptInstance::get_property_list(List<PropertyInfo> *p_properties) const{

	for (const Map<StringName,VisualScript::Variable>::Element *E=script->variables.front();E;E=E->next()) {

		PropertyInfo p = E->get().info;
		p.name=SCRIPT_VARIABLES_PREFIX+String(E->key());
		p_properties->push_back(p);

	}
}
Variant::Type VisualScriptInstance::get_property_type(const StringName& p_name,bool *r_is_valid) const{

	const Map<StringName,StringName>::Element *remap = script->script_variable_remap.find(p_name);
	if (!remap) {
		if (r_is_valid)
			*r_is_valid=false;
		return Variant::NIL;
	}

	const Map<StringName,VisualScript::Variable>::Element *E=script->variables.find(remap->get());
	if (!E) {
		if (r_is_valid)
			*r_is_valid=false;
		ERR_FAIL_V(Variant::NIL);
	}

	if (r_is_valid)
		*r_is_valid=true;

	return E->get().info.type;

}

void VisualScriptInstance::get_method_list(List<MethodInfo> *p_list) const{

	for (const Map<StringName,VisualScript::Function>::Element *E=script->functions.front();E;E=E->next()) {

		MethodInfo mi;
		mi.name=E->key();
		if (E->get().function_id>=0 && E->get().nodes.has(E->get().function_id)) {

			Ref<VisualScriptFunction> vsf = E->get().nodes[E->get().function_id].node;
			if (vsf.is_valid()) {

				for(int i=0;i<vsf->get_argument_count();i++) {
					PropertyInfo arg;
					arg.name=vsf->get_argument_name(i);
					arg.type=vsf->get_argument_type(i);

					mi.arguments.push_back(arg);
				}

				//vsf->Get_ for now at least it does not return..
			}
		}

		p_list->push_back(mi);
	}

}
bool VisualScriptInstance::has_method(const StringName& p_method) const{

	return script->functions.has(p_method);
}


//#define VSDEBUG(m_text) print_line(m_text)
#define VSDEBUG(m_text)

Variant VisualScriptInstance::call(const StringName& p_method,const Variant** p_args,int p_argcount,Variant::CallError &r_error){

	r_error.error=Variant::CallError::CALL_OK; //ok by default

	Map<StringName,Function>::Element *F = functions.find(p_method);
	if (!F) {
		r_error.error=Variant::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	VSDEBUG("CALLING: "+String(p_method));

	Function *f=&F->get();

	int total_stack_size=0;

	total_stack_size+=f->max_stack*sizeof(Variant); //variants
	total_stack_size+=f->node_count*sizeof(bool);
	total_stack_size+=(max_input_args+max_output_args)*sizeof(Variant*); //arguments
	total_stack_size+=f->flow_stack_size*sizeof(int); //flow

	VSDEBUG("STACK SIZE: "+itos(total_stack_size));
	VSDEBUG("STACK VARIANTS: : "+itos(f->max_stack));
	VSDEBUG("SEQBITS: : "+itos(f->node_count));
	VSDEBUG("MAX INPUT: "+itos(max_input_args));
	VSDEBUG("MAX OUTPUT: "+itos(max_output_args));
	VSDEBUG("FLOW STACK SIZE: "+itos(f->flow_stack_size));

	void *stack = alloca(total_stack_size);

	Variant *variant_stack=(Variant*)stack;
	bool *sequence_bits = (bool*)(variant_stack + f->max_stack);
	const Variant **input_args=(const Variant**)(sequence_bits+f->node_count);
	Variant **output_args=(Variant**)(input_args + max_input_args);
	int flow_max = f->flow_stack_size;
	int* flow_stack = flow_max? (int*)(output_args + max_output_args) : (int*)NULL;

	for(int i=0;i<f->node_count;i++) {
		sequence_bits[i]=false; //all starts as false
	}


	Map<int,VisualScriptNodeInstance*>::Element *E = instances.find(f->node);
	if (!E) {
		r_error.error=Variant::CallError::CALL_ERROR_INVALID_METHOD;

		ERR_EXPLAIN("No VisualScriptFunction node in function!");
		ERR_FAIL_V(Variant());
	}

	VisualScriptNodeInstance *node = E->get();

	int flow_stack_pos=0;
	if (flow_stack)	{
		flow_stack[0]=node->get_id();
	}

	VSDEBUG("ARGUMENTS: "+itos(f->argument_count)=" RECEIVED: "+itos(p_argcount));

	if (p_argcount<f->argument_count) {
		r_error.error=Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.argument=node->get_input_port_count();

		return Variant();
	}

	if (p_argcount>f->argument_count) {
		r_error.error=Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.argument=node->get_input_port_count();

		return Variant();
	}

	//allocate variant stack
	for(int i=0;i<f->max_stack;i++) {
		memnew_placement(&variant_stack[i],Variant);
	}

	//allocate function arguments (must be copied for yield to work properly)
	for(int i=0;i<p_argcount;i++) {
		variant_stack[i]=*p_args[i];
	}

	String error_str;

	bool error=false;
	int current_node_id=f->node;
	Variant return_value;
	Variant *working_mem=NULL;
#ifdef DEBUG_ENABLED
	if (ScriptDebugger::get_singleton()) {
		VisualScriptLanguage::singleton->enter_function(this,&p_method,variant_stack,&working_mem,&current_node_id);
	}
#endif

	while(true) {

		current_node_id=node->get_id();

		VSDEBUG("==========AT NODE: "+itos(current_node_id)+" base: "+node->get_base_node()->get_type());
		VSDEBUG("AT STACK POS: "+itos(flow_stack_pos));


		//setup working mem
		working_mem=node->working_mem_idx>=0 ? &variant_stack[node->working_mem_idx] : (Variant*)NULL;

		VSDEBUG("WORKING MEM: "+itos(node->working_mem_idx));

		if (current_node_id==f->node) {
			//if function node, set up function arguments from begining of stack

			for(int i=0;i<f->argument_count;i++) {
				input_args[i]=&variant_stack[i];
			}
		} else {
			//setup input pointers normally
			VSDEBUG("INPUT PORTS: "+itos(node->input_port_count));

			for(int i=0 ; i<node->input_port_count ; i++) {


				int index = node->input_ports[i] & VisualScriptNodeInstance::INPUT_MASK;

				if (node->input_ports[i] & VisualScriptNodeInstance::INPUT_DEFAULT_VALUE_BIT) {
					//is a default value (unassigned input port)
					input_args[i]=&default_values[index];
					VSDEBUG("\tPORT "+itos(i)+" DEFAULT VAL");
				} else if (node->input_ports[i] & VisualScriptNodeInstance::INPUT_UNSEQUENCED_READ_BIT) {
					//from a node that requires read
					Function::UnsequencedGet *ug = &f->unsequenced_gets[index];

					bool ok = ug->from->get_output_port_unsequenced(i,&variant_stack[ug->to_stack],working_mem,error_str);
					if (!ok) {
						r_error.error=Variant::CallError::CALL_ERROR_INVALID_METHOD;
						current_node_id=ug->from->get_id();
						error=true;
						working_mem=NULL;
						break;
					}

					VSDEBUG("\tPORT "+itos(i)+" UNSEQ READ TO STACK: " + itos(ug->to_stack));
					input_args[i]=&variant_stack[ug->to_stack];
				} else {
					//regular temporary in stack
					input_args[i]=&variant_stack[index];
					VSDEBUG("PORT "+itos(i)+" AT STACK "+itos(index));

				}
			}
		}

		if (error)
			break;

		//setup output pointers

		VSDEBUG("OUTPUT PORTS: "+itos(node->output_port_count));
		for(int i=0 ; i<node->output_port_count ; i++) {
			output_args[i] = &variant_stack[ node->output_ports[i] ];
			VSDEBUG("PORT "+itos(i)+" AT STACK "+itos(node->output_ports[i]));
		}

		//do step

		bool start_sequence = flow_stack && !(flow_stack[flow_stack_pos] & VisualScriptNodeInstance::FLOW_STACK_PUSHED_BIT); //if there is a push bit, it means we are continuing a sequence


		VSDEBUG("STEP - STARTSEQ: "+itos(start_sequence));

		int ret = node->step(input_args,output_args,start_sequence,working_mem,r_error,error_str);

		if (r_error.error!=Variant::CallError::CALL_OK) {
			//use error from step
			error=true;
			break;
		}

#ifdef DEBUG_ENABLED
		if (ScriptDebugger::get_singleton()) {
			// line
			bool do_break=false;

			if (ScriptDebugger::get_singleton()->get_lines_left()>0) {

				if (ScriptDebugger::get_singleton()->get_depth()<=0)
					ScriptDebugger::get_singleton()->set_lines_left( ScriptDebugger::get_singleton()->get_lines_left() -1 );
				if (ScriptDebugger::get_singleton()->get_lines_left()<=0)
					do_break=true;
			}

			if (ScriptDebugger::get_singleton()->is_breakpoint(current_node_id,source))
				do_break=true;

			if (do_break) {
				VisualScriptLanguage::singleton->debug_break("Breakpoint",true);
			}

			ScriptDebugger::get_singleton()->line_poll();

		}
#endif
		int output = ret & VisualScriptNodeInstance::STEP_MASK;

		VSDEBUG("STEP RETURN: "+itos(ret));

		if (ret & VisualScriptNodeInstance::STEP_EXIT_FUNCTION_BIT) {
			if (node->get_working_memory_size()==0) {

				r_error.error=Variant::CallError::CALL_ERROR_INVALID_METHOD;
				error_str=RTR("Return value must be assigned to first element of node working memory! Fix your node please.");
				error=true;
			} else {
				//assign from working memory, first element
				return_value=*working_mem;
			}

			VSDEBUG("EXITING FUNCTION - VALUE "+String(return_value));
			break; //exit function requested, bye
		}

		VisualScriptNodeInstance *next=NULL; //next node

		if ( (ret==output || ret&VisualScriptNodeInstance::STEP_FLAG_PUSH_STACK_BIT) && node->sequence_output_count) {
			//if no exit bit was set, and has sequence outputs, guess next node
			if (output<0 || output>=node->sequence_output_count) {
				r_error.error=Variant::CallError::CALL_ERROR_INVALID_METHOD;
				error_str=RTR("Node returned an invalid sequence output: ")+itos(output);
				error=true;
				break;
			}

			next = node->sequence_outputs[output];
			if (next)
				VSDEBUG("GOT NEXT NODE - "+itos(next->get_id()));
			else
				VSDEBUG("GOT NEXT NODE - NULL");
		}

		if (flow_stack) {

			//update flow stack pos (may have changed)
			flow_stack[flow_stack_pos] = current_node_id;

			//add stack push bit if requested
			if (ret & VisualScriptNodeInstance::STEP_FLAG_PUSH_STACK_BIT) {

				flow_stack[flow_stack_pos] |= VisualScriptNodeInstance::FLOW_STACK_PUSHED_BIT;
				sequence_bits[ node ->sequence_index ]=true; //remember sequence bit
				VSDEBUG("NEXT SEQ - FLAG BIT");
			} else {
				sequence_bits[ node ->sequence_index ]=false; //forget sequence bit
				VSDEBUG("NEXT SEQ - NORMAL");
			}


			if (ret & VisualScriptNodeInstance::STEP_FLAG_GO_BACK_BIT) {
				//go back request

				if (flow_stack_pos>0) {
					flow_stack_pos--;
					node = instances[ flow_stack[flow_stack_pos] & VisualScriptNodeInstance::FLOW_STACK_MASK ];
					VSDEBUG("NEXT IS GO BACK");
				} else {
					VSDEBUG("NEXT IS GO BACK, BUT NO NEXT SO EXIT");
					break; //simply exit without value or error
				}
			} else if (next) {


				if (sequence_bits[next->sequence_index]) {
					// what happened here is that we are entering a node that is in the middle of doing a sequence (pushed stack) from the front
					// because each node has a working memory, we can't really do a sub-sequence
					// as a result, the sequence will be restarted and the stack will roll back to find where this node
					// started the sequence

					bool found = false;

					for(int i=flow_stack_pos;i>=0;i--) {


						if ( (flow_stack[i] & VisualScriptNodeInstance::FLOW_STACK_MASK ) == next->get_id() ) {
							flow_stack_pos=i; //roll back and remove bit
							flow_stack[i]=next->get_id();
							sequence_bits[next->sequence_index]=false;
							found=true;
						}
					}

					if (!found) {
						r_error.error=Variant::CallError::CALL_ERROR_INVALID_METHOD;
						error_str=RTR("Found sequence bit but not the node in the stack, report bug!");
						error=true;
						break;
					}

					node=next;
					VSDEBUG("RE-ENTERED A LOOP, RETURNED STACK POS TO - "+itos(flow_stack_pos));

				} else {
					// check for stack overflow
					if (flow_stack_pos+1 >= flow_max) {
						r_error.error=Variant::CallError::CALL_ERROR_INVALID_METHOD;
						error_str=RTR("Stack overflow with stack depth: ")+itos(output);
						error=true;
						break;
					}

					node = next;

					flow_stack_pos++;
					flow_stack[flow_stack_pos]=node->get_id();

					VSDEBUG("INCREASE FLOW STACK");

				}

			} else {
				//no next node, try to go back in stack to pushed bit

				bool found = false;

				for(int i=flow_stack_pos;i>=0;i--) {

					VSDEBUG("FS "+itos(i)+" - "+itos(flow_stack[i]));
					if (flow_stack[i] & VisualScriptNodeInstance::FLOW_STACK_PUSHED_BIT) {

						node = instances[ flow_stack[i] & VisualScriptNodeInstance::FLOW_STACK_MASK ];
						flow_stack_pos=i;
						found=true;
					}
				}

				if (!found) {
					VSDEBUG("NO NEXT NODE, NO GO BACK, EXITING");
					break; //done, couldn't find a push stack bit
				}

				VSDEBUG("NO NEXT NODE, GO BACK TO: "+itos(flow_stack_pos));

			}
		} else {

			node=next; //stackless mode, simply assign next node
		}

	}



	if (error) {

		//error
		// function, file, line, error, explanation
		String err_file = script->get_path();
		String err_func = p_method;
		int err_line=current_node_id; //not a line but it works as one


		//if (!GDScriptLanguage::get_singleton()->debug_break(err_text,false)) {
			// debugger break did not happen

		if (!VisualScriptLanguage::singleton->debug_break(error_str,false)) {

			_err_print_error(err_func.utf8().get_data(),err_file.utf8().get_data(),err_line,error_str.utf8().get_data(),ERR_HANDLER_SCRIPT);
		}

		//}
	} else {


		//return_value=
	}

#ifdef DEBUG_ENABLED
	if (ScriptDebugger::get_singleton()) {
		VisualScriptLanguage::singleton->exit_function();
	}
#endif

	//clean up variant stack
	for(int i=0;i<f->max_stack;i++) {
		variant_stack[i].~Variant();
	}


	return return_value;
}

void VisualScriptInstance::notification(int p_notification){

	//do nothing as this is called using virtual

	Variant what=p_notification;
	const Variant*whatp=&what;
	Variant::CallError ce;
	call(VisualScriptLanguage::singleton->notification,&whatp,1,ce); //do as call

}

Ref<Script> VisualScriptInstance::get_script() const{

	return script;
}


void VisualScriptInstance::create(const Ref<VisualScript>& p_script,Object *p_owner) {

	script=p_script;
	owner=p_owner;
	source=p_script->get_path();

	max_input_args = 0;
	max_output_args = 0;

	for(const Map<StringName,VisualScript::Variable>::Element *E=script->variables.front();E;E=E->next()) {
		variables[E->key()]=E->get().default_value;
	}


	for(const Map<StringName,VisualScript::Function>::Element *E=script->functions.front();E;E=E->next()) {

		Function function;
		function.node=E->get().function_id;
		function.max_stack=0;
		function.flow_stack_size=0;
		function.node_count=0;

		if (function.node<0) {
			VisualScriptLanguage::singleton->debug_break_parse(get_script()->get_path(),0,"No start node in function: "+String(E->key()));

			ERR_CONTINUE( function.node < 0 );
		}

		{
			Ref<VisualScriptFunction> func_node = script->get_node(E->key(),E->get().function_id);

			if (func_node.is_null()) {
				VisualScriptLanguage::singleton->debug_break_parse(get_script()->get_path(),0,"No VisualScriptFunction typed start node in function: "+String(E->key()));
			}

			ERR_CONTINUE( !func_node.is_valid() );

			function.argument_count=func_node->get_argument_count();
			function.max_stack+=function.argument_count;
			function.flow_stack_size= func_node->is_stack_less() ? 0 : func_node->get_stack_size();

		}

		//multiple passes are required to set up this complex thing..




		//first create the nodes
		for (const Map<int,VisualScript::Function::NodeData>::Element *F=E->get().nodes.front();F;F=F->next()) {

			Ref<VisualScriptNode> node = F->get().node;
			VisualScriptNodeInstance *instance = node->instance(this); //create instance
			ERR_FAIL_COND(!instance);

			instance->base=node.ptr();

			instance->id=F->key();
			instance->input_port_count = node->get_input_value_port_count();
			instance->output_port_count = node->get_output_value_port_count();
			instance->sequence_output_count = node->get_output_sequence_port_count();
			instance->sequence_index=function.node_count++;

			if (instance->input_port_count) {
				instance->input_ports = memnew_arr(int,instance->input_port_count);
				for(int i=0;i<instance->input_port_count;i++) {
					instance->input_ports[i]=-1; //if not assigned, will become default value
				}
			}

			if (instance->output_port_count) {
				instance->output_ports = memnew_arr(int,instance->output_port_count);
				for(int i=0;i<instance->output_port_count;i++) {
					instance->output_ports[i]=-1; //if not assigned, will output to trash
				}
			}

			if (instance->sequence_output_count) {
				instance->sequence_outputs = memnew_arr(VisualScriptNodeInstance*,instance->sequence_output_count);
				for(int i=0;i<instance->sequence_output_count;i++) {
					instance->sequence_outputs[i]=NULL; //if it remains null, flow ends here
				}
			}

			if (instance->get_working_memory_size()) {
				instance->working_mem_idx = function.max_stack;
				function.max_stack+=instance->get_working_memory_size();
			} else {
				instance->working_mem_idx=-1; //no working mem
			}

			max_input_args = MAX( max_input_args, instance->input_port_count );
			max_output_args = MAX( max_output_args, instance->output_port_count );

			instances[F->key()]=instance;


		}

		function.trash_pos = function.max_stack++; //create pos for trash

		//second pass, do data connections

		for(const Set<VisualScript::DataConnection>::Element *F=E->get().data_connections.front();F;F=F->next()) {

			VisualScript::DataConnection dc = F->get();
			ERR_CONTINUE(!instances.has(dc.from_node));
			VisualScriptNodeInstance *from = instances[dc.from_node];
			ERR_CONTINUE(!instances.has(dc.to_node));
			VisualScriptNodeInstance *to = instances[dc.to_node];
			ERR_CONTINUE(dc.from_port >= from->output_port_count);
			ERR_CONTINUE(dc.to_port >= to->input_port_count);

			if (from->output_ports[dc.from_port]==-1) {

				int stack_pos = function.max_stack++;
				from->output_ports[dc.from_port] = stack_pos;
			}


			if (from->is_output_port_unsequenced(dc.from_node)) {

				//prepare an unsequenced read (must actually get the value from the output)
				int stack_pos = function.max_stack++;

				Function::UnsequencedGet uget;
				uget.from=from;
				uget.from_port=dc.from_port;
				uget.to_stack=stack_pos;

				to->input_ports[dc.to_port] = function.unsequenced_gets.size() | VisualScriptNodeInstance::INPUT_UNSEQUENCED_READ_BIT;
				function.unsequenced_gets.push_back(uget);

			} else {

				to->input_ports[dc.to_port] = from->output_ports[dc.from_port]; //read from wherever the stack is
			}

		}

		//third pass, do sequence connections

		for(const Set<VisualScript::SequenceConnection>::Element *F=E->get().sequence_connections.front();F;F=F->next()) {

			VisualScript::SequenceConnection sc = F->get();
			ERR_CONTINUE(!instances.has(sc.from_node));
			VisualScriptNodeInstance *from = instances[sc.from_node];
			ERR_CONTINUE(!instances.has(sc.to_node));
			VisualScriptNodeInstance *to = instances[sc.to_node];
			ERR_CONTINUE(sc.from_output >= from->sequence_output_count);

			from->sequence_outputs[sc.from_output]=to;

		}

		//fourth pass:
		// 1) unassigned input ports to default values
		// 2) connect unassigned output ports to trash


		for (const Map<int,VisualScript::Function::NodeData>::Element *F=E->get().nodes.front();F;F=F->next()) {

			ERR_CONTINUE(!instances.has(F->key()));

			Ref<VisualScriptNode> node = F->get().node;
			VisualScriptNodeInstance *instance = instances[F->key()];

			// conect to default values
			for(int i=0;i<instance->input_port_count;i++) {
				if (instance->input_ports[i]==-1) {

					//unassigned, connect to default val
					instance->input_ports[i] = default_values.size() | VisualScriptNodeInstance::INPUT_DEFAULT_VALUE_BIT;
					default_values.push_back( node->get_default_input_value(i) );
				}
			}

			// conect to trash
			for(int i=0;i<instance->output_port_count;i++) {
				if (instance->output_ports[i]==-1) {
					instance->output_ports[i] = function.trash_pos; //trash is same for all
				}
			}
		}


		functions[E->key()]=function;
	}
}

ScriptLanguage *VisualScriptInstance::get_language(){

	return VisualScriptLanguage::singleton;
}


VisualScriptInstance::VisualScriptInstance() {


}

VisualScriptInstance::~VisualScriptInstance() {

	if (VisualScriptLanguage::singleton->lock)
		VisualScriptLanguage::singleton->lock->lock();

	script->instances.erase(owner);

	if (VisualScriptLanguage::singleton->lock)
		VisualScriptLanguage::singleton->lock->unlock();

	for (Map<int,VisualScriptNodeInstance*>::Element *E=instances.front();E;E=E->next()) {
		memdelete(E->get());
	}
}









///////////////////////////////////////////////

String VisualScriptLanguage::get_name() const {

	return "VisualScript";
}

/* LANGUAGE FUNCTIONS */
void VisualScriptLanguage::init() {


}
String VisualScriptLanguage::get_type() const {

	return "VisualScript";
}
String VisualScriptLanguage::get_extension() const {

	return "vs";
}
Error VisualScriptLanguage::execute_file(const String& p_path) {

	return OK;
}
void VisualScriptLanguage::finish() {


}

/* EDITOR FUNCTIONS */
void VisualScriptLanguage::get_reserved_words(List<String> *p_words) const {


}
void VisualScriptLanguage::get_comment_delimiters(List<String> *p_delimiters) const {


}
void VisualScriptLanguage::get_string_delimiters(List<String> *p_delimiters) const {


}
Ref<Script> VisualScriptLanguage::get_template(const String& p_class_name, const String& p_base_class_name) const {

	Ref<VisualScript> script;
	script.instance();
	script->set_instance_base_type(p_base_class_name);
	return script;
}
bool VisualScriptLanguage::validate(const String& p_script, int &r_line_error,int &r_col_error,String& r_test_error, const String& p_path,List<String> *r_functions) const {

	return false;
}
Script *VisualScriptLanguage::create_script() const {

	return memnew( VisualScript );
}
bool VisualScriptLanguage::has_named_classes() const {

	return false;
}
int VisualScriptLanguage::find_function(const String& p_function,const String& p_code) const {

	return -1;
}
String VisualScriptLanguage::make_function(const String& p_class,const String& p_name,const StringArray& p_args) const {

	return String();
}

void VisualScriptLanguage::auto_indent_code(String& p_code,int p_from_line,int p_to_line) const {


}
void VisualScriptLanguage::add_global_constant(const StringName& p_variable,const Variant& p_value) {


}


/* DEBUGGER FUNCTIONS */



bool VisualScriptLanguage::debug_break_parse(const String& p_file, int p_node,const String& p_error) {
	//break because of parse error

    if (ScriptDebugger::get_singleton() && Thread::get_caller_ID()==Thread::get_main_ID()) {

	_debug_parse_err_node=p_node;
	_debug_parse_err_file=p_file;
	_debug_error=p_error;
	ScriptDebugger::get_singleton()->debug(this,false);
	return true;
    } else {
	return false;
    }

}

bool VisualScriptLanguage::debug_break(const String& p_error,bool p_allow_continue) {

    if (ScriptDebugger::get_singleton() && Thread::get_caller_ID()==Thread::get_main_ID()) {

	_debug_parse_err_node=-1;
	_debug_parse_err_file="";
	_debug_error=p_error;
	ScriptDebugger::get_singleton()->debug(this,p_allow_continue);
	return true;
    } else {
	return false;
    }

}


String VisualScriptLanguage::debug_get_error() const {

    return _debug_error;
}

int VisualScriptLanguage::debug_get_stack_level_count() const {

	if (_debug_parse_err_node>=0)
		return 1;


	return _debug_call_stack_pos;
}
int VisualScriptLanguage::debug_get_stack_level_line(int p_level) const {

	if (_debug_parse_err_node>=0)
		return _debug_parse_err_node;

    ERR_FAIL_INDEX_V(p_level,_debug_call_stack_pos,-1);

    int l = _debug_call_stack_pos - p_level -1;

    return *(_call_stack[l].current_id);

}
String VisualScriptLanguage::debug_get_stack_level_function(int p_level) const {

	if (_debug_parse_err_node>=0)
		return "";

    ERR_FAIL_INDEX_V(p_level,_debug_call_stack_pos,"");
    int l = _debug_call_stack_pos - p_level -1;
    return *_call_stack[l].function;
}
String VisualScriptLanguage::debug_get_stack_level_source(int p_level) const {

	if (_debug_parse_err_node>=0)
		return _debug_parse_err_file;

    ERR_FAIL_INDEX_V(p_level,_debug_call_stack_pos,"");
    int l = _debug_call_stack_pos - p_level -1;
    return _call_stack[l].instance->get_script_ptr()->get_path();

}
void VisualScriptLanguage::debug_get_stack_level_locals(int p_level,List<String> *p_locals, List<Variant> *p_values, int p_max_subitems,int p_max_depth) {

	if (_debug_parse_err_node>=0)
		return;

	ERR_FAIL_INDEX(p_level,_debug_call_stack_pos);

	int l = _debug_call_stack_pos - p_level -1;
	const StringName *f = _call_stack[l].function;

	ERR_FAIL_COND(!_call_stack[l].instance->functions.has(*f));
	VisualScriptInstance::Function *func = &_call_stack[l].instance->functions[*f];

	VisualScriptNodeInstance *node =_call_stack[l].instance->instances[*_call_stack[l].current_id];
	ERR_FAIL_COND(!node);

	p_locals->push_back("node_name");
	p_values->push_back(node->get_base_node()->get_text());

	for(int i=0;i<node->input_port_count;i++) {
		String name = node->get_base_node()->get_input_value_port_info(i).name;
		if (name==String()) {
			name="in_"+itos(i);
		}

		p_locals->push_back("input/"+name);

		//value is trickier

		int in_from = node->input_ports[i];
		int in_value = in_from&VisualScriptNodeInstance::INPUT_MASK;

		if (in_from&VisualScriptNodeInstance::INPUT_DEFAULT_VALUE_BIT) {
			p_values->push_back(_call_stack[l].instance->default_values[in_value]);
		} else if (in_from&VisualScriptNodeInstance::INPUT_UNSEQUENCED_READ_BIT) {
			p_values->push_back( _call_stack[l].stack[ func->unsequenced_gets[ in_value ].to_stack ] );
		} else {
			p_values->push_back( _call_stack[l].stack[ in_value] );
		}
	}

	for(int i=0;i<node->output_port_count;i++) {

		String name = node->get_base_node()->get_output_value_port_info(i).name;
		if (name==String()) {
			name="out_"+itos(i);
		}

		p_locals->push_back("output/"+name);

		//value is trickier

		int in_from = node->output_ports[i];
		p_values->push_back( _call_stack[l].stack[ in_from] );

	}

	for(int i=0;i<node->get_working_memory_size();i++) {
		p_locals->push_back("working_mem/mem_"+itos(i));
		p_values->push_back( (*_call_stack[l].work_mem)[i]);
	}

/*
    ERR_FAIL_INDEX(p_level,_debug_call_stack_pos);


    VisualFunction *f = _call_stack[l].function;

    List<Pair<StringName,int> > locals;

    f->debug_get_stack_member_state(*_call_stack[l].line,&locals);
    for( List<Pair<StringName,int> >::Element *E = locals.front();E;E=E->next() ) {

	p_locals->push_back(E->get().first);
	p_values->push_back(_call_stack[l].stack[E->get().second]);
    }
*/
}
void VisualScriptLanguage::debug_get_stack_level_members(int p_level,List<String> *p_members, List<Variant> *p_values, int p_max_subitems,int p_max_depth) {

	if (_debug_parse_err_node>=0)
		return;

	ERR_FAIL_INDEX(p_level,_debug_call_stack_pos);
	int l = _debug_call_stack_pos - p_level -1;


	Ref<VisualScript> vs = _call_stack[l].instance->get_script();
	if (vs.is_null())
		return;

	List<StringName> vars;
	vs->get_variable_list(&vars);
	for (List<StringName>::Element *E=vars.front();E;E=E->next()) {
		Variant v;
		if (_call_stack[l].instance->get_variable(E->get(),&v)) {
			p_members->push_back("variables/"+E->get());
			p_values->push_back(v);
		}
	}
}

void VisualScriptLanguage::debug_get_globals(List<String> *p_locals, List<Variant> *p_values, int p_max_subitems,int p_max_depth) {

    //no globals are really reachable in gdscript
}
String VisualScriptLanguage::debug_parse_stack_level_expression(int p_level,const String& p_expression,int p_max_subitems,int p_max_depth) {

	if (_debug_parse_err_node>=0)
		return "";
	return "";
}



void VisualScriptLanguage::reload_all_scripts() {


}
void VisualScriptLanguage::reload_tool_script(const Ref<Script>& p_script,bool p_soft_reload) {


}
/* LOADER FUNCTIONS */

void VisualScriptLanguage::get_recognized_extensions(List<String> *p_extensions) const {

	p_extensions->push_back("vs");

}
void VisualScriptLanguage::get_public_functions(List<MethodInfo> *p_functions) const {


}
void VisualScriptLanguage::get_public_constants(List<Pair<String,Variant> > *p_constants) const {


}

void VisualScriptLanguage::profiling_start() {


}
void VisualScriptLanguage::profiling_stop() {


}

int VisualScriptLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr,int p_info_max) {

	return 0;
}

int VisualScriptLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr,int p_info_max) {

	return 0;
}


VisualScriptLanguage* VisualScriptLanguage::singleton=NULL;


void VisualScriptLanguage::add_register_func(const String& p_name,VisualScriptNodeRegisterFunc p_func) {

	ERR_FAIL_COND(register_funcs.has(p_name));
	register_funcs[p_name]=p_func;
}

Ref<VisualScriptNode> VisualScriptLanguage::create_node_from_name(const String& p_name) {

	ERR_FAIL_COND_V(!register_funcs.has(p_name),Ref<VisualScriptNode>());

	return register_funcs[p_name](p_name);
}

void VisualScriptLanguage::get_registered_node_names(List<String> *r_names) {

	for (Map<String,VisualScriptNodeRegisterFunc>::Element *E=register_funcs.front();E;E=E->next()) {
		r_names->push_back(E->key());
	}
}


VisualScriptLanguage::VisualScriptLanguage() {

	notification="_notification";
	singleton=this;
#ifndef NO_THREADS
	lock = Mutex::create();
#endif


	_debug_parse_err_node=-1;
	_debug_parse_err_file="";
	_debug_call_stack_pos=0;
	int dmcs=GLOBAL_DEF("debug/script_max_call_stack",1024);
	if (ScriptDebugger::get_singleton()) {
		//debugging enabled!
		_debug_max_call_stack = dmcs;
		if (_debug_max_call_stack<1024)
			_debug_max_call_stack=1024;
		_call_stack = memnew_arr( CallLevel, _debug_max_call_stack+1 );

	} else {
		_debug_max_call_stack=0;
		_call_stack=NULL;
	}

}

VisualScriptLanguage::~VisualScriptLanguage() {

	if (lock)
		memdelete(lock);

	if (_call_stack)  {
		memdelete_arr(_call_stack);
	}
	singleton=NULL;
}