#include "dlink.h"
#include "limits.h"
#include "multiclass.h"

#include <algorithm>


/******************************************************************************************************************
*******************************************************************************************************************
												Link Models
*******************************************************************************************************************
******************************************************************************************************************/


/*						Multiclass CTM Functions
			(currently only for car & truck two classes)
	(see: Z. (Sean) Qian et al./Trans. Res. Part B 99 (2017) 183-204)			
**************************************************************************/

MNM_Dlink_Ctm_Multiclass::MNM_Dlink_Ctm_Multiclass(TInt ID,
												   TInt number_of_lane,
												   TFlt length, // (m)
												   TFlt lane_hold_cap_car, // Jam density (veh/m)
												   TFlt lane_hold_cap_truck,
												   TFlt lane_flow_cap_car, // Max flux (veh/s)
												   TFlt lane_flow_cap_truck,
												   TFlt ffs_car, // Free-flow speed (m/s)
												   TFlt ffs_truck, 
												   TFlt unit_time, // (s)
												   TFlt flow_scalar) // flow_scalar can be 2.0, 5.0, 10.0, etc.
	: MNM_Dlink::MNM_Dlink(ID, number_of_lane, length, 0.0) // Note: m_ffs is not used in child class, so let it be 0.0
{
	// Jam density for private cars and trucks cannot be negative
	if ((lane_hold_cap_car < 0) || (lane_hold_cap_truck < 0)){
		printf("lane_hold_cap can't be negative, current link ID is %d\n", m_link_ID());
		exit(-1);
	}
	// Jam density for private cars cannot be too large
	if (lane_hold_cap_car > TFlt(300) / TFlt(1600)){
		// "lane_hold_cap is too large, set to 300 veh/mile
		lane_hold_cap_car = TFlt(300) / TFlt(1600);
	}
	// Jam density for trucks cannot be too large !!!NEED CHECK FOR THRESHOLD!!!
	if (lane_hold_cap_truck > TFlt(300) / TFlt(1600)){
		// "lane_hold_cap is too large, set to 300 veh/mile
		lane_hold_cap_truck = TFlt(300) / TFlt(1600);
	}

	// Maximum flux for private cars and trucks cannot be negative
	if ((lane_flow_cap_car < 0) || (lane_flow_cap_truck < 0)){
		printf("lane_flow_cap can't be less than zero, current link ID is %d\n", m_link_ID());
		exit(-1);
	}
	// Maximum flux for private cars cannot be too large
	if (lane_flow_cap_car > TFlt(3500) / TFlt(3600)){
		// lane_flow_cap is too large, set to 3500 veh/hour
		lane_flow_cap_car = TFlt(3500) / TFlt(3600);
	}
	// Maximum flux for trucks cannot be too large !!!NEED CHECK FOR THRESHOLD!!!
	if (lane_flow_cap_car > TFlt(3500) / TFlt(3600)){
		// lane_flow_cap is too large, set to 3500 veh/hour
		lane_flow_cap_car = TFlt(3500) / TFlt(3600);
	}

	m_lane_flow_cap_car = lane_flow_cap_car;
	m_lane_flow_cap_truck = lane_flow_cap_truck;	
	m_lane_hold_cap_car = lane_hold_cap_car;
	m_lane_hold_cap_truck = lane_hold_cap_truck;
	m_ffs_car = ffs_car;
	m_ffs_truck = ffs_truck;
	m_flow_scalar = flow_scalar;

	m_cell_array = std::vector<Ctm_Cell_Multiclass*>();

	// Note m_ffs_car > m_ffs_truck, so use ffs_car to define the standard cell length
	TFlt _std_cell_length = m_ffs_car * unit_time;
	m_num_cells = TInt(floor(m_length / _std_cell_length));
	if (m_num_cells == 0){
		m_num_cells = 1;
	}
	TFlt _last_cell_length = m_length - TFlt(m_num_cells - 1) * _std_cell_length;

	m_lane_critical_density_car = m_lane_flow_cap_car / m_ffs_car;
	m_lane_critical_density_truck = m_lane_flow_cap_truck / m_ffs_truck;
	m_wave_speed_car = m_lane_flow_cap_car / (m_lane_hold_cap_car - m_lane_critical_density_car);
	m_wave_speed_truck = m_lane_flow_cap_truck / (m_lane_hold_cap_truck - m_lane_critical_density_truck);

	// see the reference paper for definition
	m_lane_rho_1_N = m_lane_hold_cap_car * (m_wave_speed_car / (m_ffs_truck + m_wave_speed_car));

	init_cell_array(unit_time, _std_cell_length, _last_cell_length);
}

MNM_Dlink_Ctm_Multiclass::~MNM_Dlink_Ctm_Multiclass()
{
	for (Ctm_Cell_Multiclass* _cell : m_cell_array){
		delete _cell;
	}
	m_cell_array.clear();
}

int MNM_Dlink_Ctm_Multiclass::init_cell_array(TFlt unit_time, 
											  TFlt std_cell_length, 
											  TFlt last_cell_length)
{
	// All previous cells
	Ctm_Cell_Multiclass *cell = NULL;
	for (int i = 0; i < m_num_cells - 1; ++i){
		cell = new Ctm_Cell_Multiclass(std_cell_length,
									   unit_time,
									   // Covert lane parameters to cell (link) parameters by multiplying # of lanes
									   TFlt(m_number_of_lane) * m_lane_hold_cap_car,
									   TFlt(m_number_of_lane) * m_lane_hold_cap_truck,
									   TFlt(m_number_of_lane) * m_lane_critical_density_car,
									   TFlt(m_number_of_lane) * m_lane_critical_density_truck,
									   TFlt(m_number_of_lane) * m_lane_rho_1_N,
									   TFlt(m_number_of_lane) * m_lane_flow_cap_car,
									   TFlt(m_number_of_lane) * m_lane_flow_cap_truck,
									   m_ffs_car,
									   m_ffs_truck,
									   m_wave_speed_car,
									   m_wave_speed_truck,
									   m_flow_scalar);
		if (cell == NULL) {
			printf("Fail to initialize some standard cell.\n");
			exit(-1);
		}
		m_cell_array.push_back(cell);
	}

	// The last cell
	if (m_length > 0.0) {
		cell = new Ctm_Cell_Multiclass(last_cell_length, // Note last cell length is longer but < 2X
									   unit_time,
									   TFlt(m_number_of_lane) * m_lane_hold_cap_car,
									   TFlt(m_number_of_lane) * m_lane_hold_cap_truck,
									   TFlt(m_number_of_lane) * m_lane_critical_density_car,
									   TFlt(m_number_of_lane) * m_lane_critical_density_truck,
									   TFlt(m_number_of_lane) * m_lane_rho_1_N,
									   TFlt(m_number_of_lane) * m_lane_flow_cap_car,
									   TFlt(m_number_of_lane) * m_lane_flow_cap_truck,
									   m_ffs_car,
									   m_ffs_truck,
									   m_wave_speed_car,
									   m_wave_speed_truck,
									   m_flow_scalar);
		if (cell == NULL) {
			printf("Fail to initialize the last cell.\n");
			exit(-1);
		}
		m_cell_array.push_back(cell);
	}

	// compress the cell_array to reduce space
	m_cell_array.shrink_to_fit();

	return 0;
}

void MNM_Dlink_Ctm_Multiclass::print_info()
{
	printf("Total number of cell: \t%d\n Flow scalar: \t%.4f\n",
			int(m_num_cells), double(m_flow_scalar));
	printf("Volume for each cell is:\n");
	for (int i = 0; i < m_num_cells - 1; ++i){
		printf("%d, ", int(m_cell_array[i] -> m_volume));
	}
	printf("%d\n", int(m_cell_array[m_num_cells - 1] -> m_volume));
}

int MNM_Dlink_Ctm_Multiclass::update_out_veh()
{
	TFlt _temp_out_flux_car, _supply_car, _demand_car;
	TFlt _temp_out_flux_truck, _supply_truck, _demand_truck;
	// no update is needed if only one cell
	if (m_num_cells > 1){
		for (int i = 0; i < m_num_cells - 1; ++i){
			// car, veh_type = TInt(1)
			_demand_car = m_cell_array[i] -> get_perceived_demand(TInt(1));
			_supply_car = m_cell_array[i + 1] -> get_perceived_supply(TInt(1));
			_temp_out_flux_car = m_cell_array[i] -> m_space_fraction_car * MNM_Ults::min(_demand_car, _supply_car);
			m_cell_array[i] -> m_out_veh_car = MNM_Ults::round(_temp_out_flux_car * m_unit_time * m_flow_scalar);

			// truck, veh_type = TInt(2)
			_demand_truck = m_cell_array[i] -> get_perceived_demand(TInt(2));
			_supply_truck = m_cell_array[i + 1] -> get_perceived_supply(TInt(2));
			_temp_out_flux_truck = m_cell_array[i] -> m_space_fraction_truck * MNM_Ults::min(_demand_truck, _supply_truck);
			m_cell_array[i] -> m_out_veh_truck = MNM_Ults::round(_temp_out_flux_truck * m_unit_time * m_flow_scalar);
		}
	}
	m_cell_array[m_num_cells - 1] -> m_out_veh_car = m_cell_array[m_num_cells - 1] -> m_veh_queue_car.size();
	m_cell_array[m_num_cells - 1] -> m_out_veh_truck = m_cell_array[m_num_cells - 1] -> m_veh_queue_truck.size();
	return 0;
}

int MNM_Dlink_Ctm_Multiclass::evolve(TInt timestamp)
{
	update_out_veh();
	
	TInt _num_veh_tomove_car, _num_veh_tomove_truck;
	/* previous cells */
	if (m_num_cells > 1){
		for (int i = 0; i < m_num_cells - 1; ++i)
		{
			// Car
			_num_veh_tomove_car = m_cell_array[i] -> m_out_veh_car;
			move_veh_queue( &(m_cell_array[i] -> m_veh_queue_car),
							&(m_cell_array[i+1] -> m_veh_queue_car),
							_num_veh_tomove_car);
			// Truck
			_num_veh_tomove_truck = m_cell_array[i] -> m_out_veh_truck;
			move_veh_queue( &(m_cell_array[i] -> m_veh_queue_truck),
							&(m_cell_array[i+1] -> m_veh_queue_truck),
							_num_veh_tomove_truck);
		}
	}
	/* last cell */
	move_last_cell();

	/* update volume */
	if (m_num_cells > 1){
		for (int i = 0; i < m_num_cells - 1; ++i)
		{
			m_cell_array[i] -> m_volume_car = m_cell_array[i] -> m_veh_queue_car.size();
			m_cell_array[i] -> m_volume_truck = m_cell_array[i] -> m_veh_queue_truck.size();
			// Update perceived density of the i-th cell
			m_cell_array[i] -> update_perceived_density();
		}
	}
	m_cell_array[m_num_cells - 1] -> m_volume_car = 
		m_cell_array[m_num_cells - 1] -> m_veh_queue_car.size() + m_finished_array.size() ???;
	m_cell_array[m_num_cells - 1] -> m_volume_truck = 
		m_cell_array[m_num_cells - 1] -> m_veh_queue_truck.size() + m_finished_array.size() ???;	
	m_cell_array[m_num_cells - 1] -> update_perceived_density();

	return 0;
}


int MNM_Dlink_Ctm_Multiclass::move_last_cell() 
{
	TInt _num_veh_tomove_car = m_cell_array[m_num_cells - 1] -> m_out_veh_car;
	TInt _num_veh_tomove_truck = m_cell_array[m_num_cells - 1] -> m_out_veh_truck;
	MNM_Veh* _veh;
	for (int i = 0; i < _num_veh_tomove; ++i)
	{
		_veh = m_cell_array[m_num_cells - 1] -> m_veh_queue.front();
		m_cell_array[m_num_cells - 1] -> m_veh_queue.pop_front();
		if (_veh -> has_next_link()){
			m_finished_array.push_back(_veh);
		}
		else {
			printf("Dlink_CTM_Multiclass::Some thing wrong!\n");
			exit(-1);
		}
	}
	return 0;
}

TFlt MNM_Dlink_Ctm_Multiclass::get_link_supply()
{
	m_cell_array[0];
	return 0;
}

int MNM_Dlink_Ctm_Multiclass::clear_incoming_array()
{
	// printf("link ID: %d, incoming: %d, supply: %d\n", 
	//        (int)m_link_ID, (int)m_incoming_array.size(), 
	//        (int)(get_link_supply() * m_flow_scalar));
	if (get_link_supply() * m_flow_scalar < m_incoming_array.size()){
		// LOG(WARNING) << "Wrong incoming array size";
		exit(-1);
	}
	move_veh_queue(&m_incoming_array, &(m_cell_array[0] -> m_veh_queue), m_incoming_array.size());
	
	m_cell_array[0] -> m_volume = m_cell_array[0] -> m_veh_queue.size();
	return 0;	
}


TFlt MNM_Dlink_Ctm_Multiclass::get_link_flow()
{
	TInt _total_volume = 0;
	for (int i = 0; i < m_num_cells; ++i){
		_total_volume += m_cell_array[i] -> m_volume_car + m_cell_array[i] -> m_volume_truck;
	}
	return TFlt(_total_volume) / m_flow_scalar;
}

TFlt MNM_Dlink_Ctm_Multiclass::get_link_tt()
{
	TFlt _cost, _spd;
	// get the density in veh/mile
	TFlt _rho = get_link_flow()/m_number_of_lane/m_length;
	// get the jam density
	TFlt _rhoj = m_lane_hold_cap;
	// get the critical density
	TFlt _rhok = m_lane_flow_cap/m_ffs;

	if (_rho >= _rhoj){
		_cost = MNM_Ults::max_link_cost();
	}
	else {
		if (_rho <= _rhok){
			_spd = m_ffs;
		}
		else {
			_spd = MNM_Ults::max(0.001 * m_ffs, 
					m_lane_flow_cap * (_rhoj - _rho) / (_rhoj - _rhok) / _rho);
		}
		_cost = m_length / _spd;
	}
	return _cost;
}


/*							Multiclass CTM Cells
**************************************************************************/

MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::Ctm_Cell_Multiclass(TFlt cell_length,
														TFlt unit_time,
														TFlt hold_cap_car,
														TFlt hold_cap_truck,
														TFlt critical_density_car,
														TFlt critical_density_truck, 
														TFlt rho_1_N,
														TFlt flow_cap_car,
														TFlt flow_cap_truck,
														TFlt ffs_car,
														TFlt ffs_truck,
														TFlt wave_speed_car,
														TFlt wave_speed_truck,
														TFlt flow_scalar)
{
	m_cell_length = cell_length;
	m_unit_time = unit_time
	m_flow_scalar = flow_scalar;

	m_hold_cap_car = hold_cap_car;
	m_hold_cap_truck = hold_cap_truck;
	m_critical_density_car = critical_density_car;
	m_critical_density_truck = critical_density_truck;
	m_rho_1_N = rho_1_N;
	m_flow_cap_car = flow_cap_car;
	m_flow_cap_truck = flow_cap_truck;
	m_ffs_car = ffs_car;
	m_ffs_truck = ffs_truck;
	m_wave_speed_car = wave_speed_car;
	m_wave_speed_truck = wave_speed_truck;

	m_volume_car = TInt(0);
	m_volume_truck = TInt(0);
	m_out_veh_car = TInt(0);
	m_out_veh_truck = TInt(0);
	m_veh_queue_car = std::deque<MNM_Veh*>();
	m_veh_queue_truck = std::deque<MNM_Veh*>();
}

MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::~Ctm_Cell_Multiclass()
{
	m_veh_queue_car.clear();
	m_veh_queue_truck.clear();
}

int MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::update_perceived_density()
{
	TFlt _real_volume_car = TFlt(m_volume_car) / m_flow_scalar;
	TFlt _real_volume_truck = TFlt(m_volume_truck) / m_flow_scalar;

	TFlt _density_car = _real_volume_car / m_cell_length;
	TFlt _density_truck = _real_volume_truck / m_cell_length;
	
	// Free-flow traffic (free-flow for both car and truck classes)
	if (_density_car/m_critical_density_car + _density_truck/m_critical_density_truck <= 1) {
		m_space_fraction_car = _density_car/m_critical_density_car;
		m_space_fraction_truck = _density_truck/m_critical_density_truck;
		m_perceived_density_car = _density_car + m_critical_density_car * _space_fraction_truck;
		m_perceived_density_truck = _density_truck + m_critical_density_truck * _space_fraction_car;
	}
	// Semi-congested traffic (truck free-flow but car not)d
	else if (_density_car / (1 - _density_truck/m_critical_density_truck) <= m_rho_1_N) {
		m_space_fraction_truck = _density_truck/m_critical_density_truck;
		m_space_fraction_car = 1 - _space_fraction_truck;
		m_perceived_density_car = _density_car / _space_fraction_car;
		m_perceived_density_truck = _density_truck / _space_fraction_truck;
	}
	// Fully congested traffic (both car and truck not free-flow)
	// this case should satisfy: 1. m_perceived_density_car > m_rho_1_N
	// 							 2. m_perceived_density_truck > m_critical_density_truck
	else {
		TFlt _tmp_car = m_hold_cap_car * m_wave_speed_car / _density_car;
		TFlt _tmp_truck = m_hold_cap_truck * m_wave_speed_truck / _density_truck;
		m_space_fraction_car = (m_wave_speed_car - m_wave_speed_truck + _tmp_truck) / (_tmp_car + _tmp_truck);
		m_space_fraction_truck = (m_wave_speed_truck - m_wave_speed_car + _tmp_car) / (_tmp_car + _tmp_truck);
		m_perceived_density_car = _density_car / _space_fraction_car;
		m_perceived_density_truck = _density_truck / _space_fraction_truck;
	}
	return 0;
}

TFlt MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::get_perceived_demand(TInt veh_type)
{	
	// car
	if (veh_type == TInt(1)) {
		return std::min(m_flow_cap_car, m_ffs_car * m_perceived_density_car);
	}
	// truck
	else {
		return std::min(m_flow_cap_truck, m_ffs_truck * m_perceived_density_truck);
	}
}

TFlt MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::get_perceived_supply(TInt veh_type)
{
	// car
	if (veh_type == TInt(1)) {
		TFlt _tmp = std::min(m_flow_cap_car, m_wave_speed_car * (m_hold_cap_car - m_perceived_density_car));
	}
	// truck
	else {
		TFlt _tmp = std::min(m_flow_cap_truck, m_wave_speed_truck * (m_hold_cap_truck - m_perceived_density_truck));
	}
	return std::max(0.0, _tmp);
}



/******************************************************************************************************************
*******************************************************************************************************************
												Node Models
*******************************************************************************************************************
******************************************************************************************************************/



