<?php
/*
$Id$

  This code is part of LDAP Account Manager (http://www.ldap-account-manager.org/)
  Copyright (C) 2015  Roland Gruber

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

/**
* Manages memberships in NIS net groups.
*
* @package modules
* @author Roland Gruber
*/

/**
* Manages memberships in NIS net groups.
*
* @package modules
*/
class nisNetGroupUser extends baseModule {

	/** list of NIS netgroups the user is memberOf (array(array(name => '', dn => '', host => '', domain => ''))) */
	private $groups = array();
	/** list of NIS netgroups the user was memberOf (array(array(name => '', dn => '', host => '', domain => ''))) */
	private $groupsOrig = array();
	/** group cache (array(array(cn => '', dn => '', nisnetgrouptriple => array()))) */
	private $groupCache = null;
	/** original uid */
	private $uidOrig = null;
	
	/**
	* Returns true if this module can manage accounts of the current type, otherwise false.
	* 
	* @return boolean true if module fits
	*/
	public function can_manage() {
		return in_array($this->get_scope(), array('user'));
	}

	/**
	* Returns meta data that is interpreted by parent class
	*
	* @return array array with meta data
	* 
	* @see baseModule::get_metaData()
	*/
	public function get_metaData() {
		$return = array();
		// icon
		$return['icon'] = 'groupBig.png';
		// module dependencies
		$return['dependencies'] = array('depends' => array(array('posixAccount', 'inetOrgPerson')), 'conflicts' => array());
		// alias name
		$return["alias"] = _("NIS net groups");
		// available PDF fields
		$return['PDF_fields']['memberships'] = _('NIS net groups');
		// help Entries
		$return['help'] = array(
			'memberships_upload' => array(
				"Headline" => _('NIS net groups'),
				"Text" => _("Here you can enter a list of net groups. Group blocks are separated by comma in format GROUP#HOST#DOMAIN. Host and domain are optional.")
			),
		);
		// upload columns
		$return['upload_columns'][] = array(
			'name' => 'nisNetGroupUser_memberships',
			'description' => _('Memberships'),
			'help' => 'memberships_upload',
			'example' => 'group1#host#domain,group2#host#domain'
		);
		return $return;
	}
	
	/**
	* This function fills the $messages variable with output messages from this module.
	*/
	function load_Messages() {
		$this->messages['host'][0] = array('ERROR', _('Host name'), _('Host name contains invalid characters. Valid characters are: a-z, A-Z, 0-9 and .-_ !'));
		$this->messages['domain'][0] = array('ERROR', _('Domain name'), _('Domain name is invalid!'));
	}

	/**
	* Initializes the module after it became part of an accountContainer
	*
	* @param string $base the name of the accountContainer object ($_SESSION[$base])
	*/
	function init($base) {
		// call parent init
		parent::init($base);
		$this->groups = array();
		$this->groupsOrig = array();
	}

	/**
	 * This function loads all needed LDAP attributes.
	 *
	 * @param array $attr list of attributes
	 */
	function load_attributes($attr) {
		parent::load_attributes($attr);
		if (empty($attr['uid'][0])) {
			return;
		}
		$uid = $attr['uid'][0];
		$this->uidOrig = $uid;
		$types = array('netgroup');
		$typeSettings = $_SESSION['config']->get_typeSettings();
		$groupList = array();
		$filter = '(&(objectClass=nisNetgroup)(nisnetgrouptriple=*))';
		if (!empty($typeSettings['filter_' . $types[0]])) {
			$typeFilter = $typeSettings['filter_' . $types[0]];
			if (strpos($typeFilter, '(') !== 0) {
				$typeFilter = '(' . $typeFilter . ')';
			}
			$filter = '(&' . $filter . $typeFilter . ')';
		}
		$groupList = searchLDAPByFilter($filter, array('dn', 'cn', 'nisnetgrouptriple'), $types);
		$this->groupsOrig = array();
		$tripleRegex = '/^\\(([^,]*),([^,]*),([^,]*)\\)$/';
		foreach ($groupList as $group) {
			foreach ($group['nisnetgrouptriple'] as $triple) {
				$matches = array();
				if (preg_match($tripleRegex, $triple, $matches) == 0) {
					continue;
				}
				$host = $matches[1];
				$user = $matches[2];
				$domain = $matches[3];
				if ($user == $uid) {
					$this->groupsOrig[] = array(
						'name' => $group['cn'][0],
						'dn' => $group['dn'],
						'host' => $host,
						'domain' => $domain
					);
				}
			}
		}
		usort($this->groupsOrig, array($this, 'sortTriple'));
		$this->groups = $this->groupsOrig;
	}

	/**
	* Displays the group selection.
	*
	* @return htmlElement meta HTML code
	*/
	public function display_html_attributes() {
		$return = new htmlTable();
		$return->addElement(new htmlOutputText(_('Group')));
		$return->addElement(new htmlOutputText(_('Host name')));
		$return->addElement(new htmlOutputText(_('Domain name')), true);
		for ($i = 0; $i < sizeof($this->groups); $i++) {
			$group = $this->groups[$i];
			$return->addElement(new htmlOutputText($group['name']));
			$return->addElement(new htmlInputField('host_' . $i, $group['host']));
			$return->addElement(new htmlInputField('domain_' . $i, $group['domain']));
			$delButton = new htmlButton('del_' . $i, 'del.png', true);
			$delButton->setTitle(_('Delete'));
			$return->addElement($delButton, true);
		}
		$return->addVerticalSpace('40px');
		
		// new entry
		$groupList = array();
		$groupData = $this->findGroups();
		if (sizeof($groupData) > 0) {
			$filterGroup = new htmlGroup();
			$filterGroup->addElement(new htmlOutputText(_('Filter') . ' '));
			$filter = new htmlInputField('group_filter');
			$filter->setFieldSize('5em');
			$filter->setOnKeyUp('filterSelect(\'group_filter\', \'group_add\', event);');
			$filterGroup->addElement($filter);
			$return->addElement($filterGroup, true);
			
			foreach ($groupData as $group) {
				$groupList[$group['cn'][0]] = $group['cn'][0] . '#+#' . $group['dn'];
			}
			$groupSelect = new htmlSelect('group_add', $groupList);
			$groupSelect->setHasDescriptiveElements(true);
			$return->addElement($groupSelect);
			$return->addElement(new htmlInputField('host_add'));
			$return->addElement(new htmlInputField('domain_add'));
			$addButton = new htmlButton('addGroup', 'add.png', true);
			$addButton->setTitle(_('Add'));
			$return->addElement($addButton, true);
		}
		return $return;
	}

	/**
	* Processes user input of the group selection page.
	* It checks if all input values are correct and updates the associated LDAP attributes.
	*
	* @return array list of info/error messages
	*/
	public function process_attributes() {
		$errors = array();
		// add new entry
		if (isset($_POST['addGroup'])) {
			$parts = explode('#+#', $_POST['group_add']);
			$this->groups[] = array(
				'name' => $parts[0],
				'dn' => $parts[1],
				'host' => $_POST['host_add'],
				'domain' => $_POST['domain_add']
			);
				if (!empty($_POST['host_add']) && !get_preg($_POST['host_add'], 'DNSname')) {
					$message = $this->messages['host'][0];
					$message[2] = $message[2] . '<br><br>' . $_POST['host_add'];
					$errors[] = $message;
				}
				if (!empty($_POST['domain_add']) && !get_preg($_POST['domain_add'], 'DNSname')) {
					$message = $this->messages['domain'][0];
					$message[2] = $message[2] . '<br><br>' . $_POST['domain_add'];
					$errors[] = $message;
				}
		}
		// check existing
		$counter = 0;
		while (isset($_POST['host_' . $counter])) {
			if (isset($_POST['del_' . $counter])) {
				unset($this->groups[$counter]);
			}
			else {
				$this->groups[$counter]['host'] = $_POST['host_' . $counter];
				if (!empty($_POST['host_' . $counter]) && !get_preg($_POST['host_' . $counter], 'DNSname')) {
					$message = $this->messages['host'][0];
					$message[2] = $message[2] . '<br><br>' . $_POST['host_' . $counter];
					$errors[] = $message;
				}
				$this->groups[$counter]['domain'] = $_POST['domain_' . $counter];
				if (!empty($_POST['domain_' . $counter]) && !get_preg($_POST['domain_' . $counter], 'DNSname')) {
					$message = $this->messages['domain'][0];
					$message[2] = $message[2] . '<br><br>' . $_POST['domain_' . $counter];
					$errors[] = $message;
				}
			}
			$counter++;
		}
		$this->groups = array_values($this->groups);
		usort($this->groups, array($this, 'sortTriple'));
		return $errors;
	}
	
	/**
	 * Returns the user ID for this user.
	 * 
	 * @return String user ID
	 */
	private function getUid() {
		$moduleAttributes = array();
		if ($this->getAccountContainer()->getAccountModule('posixAccount') != null) {
			$moduleAttributes = $this->getAccountContainer()->getAccountModule('posixAccount')->getAttributes();
		}
		else {
			$moduleAttributes = $this->getAccountContainer()->getAccountModule('inetOrgPerson')->getAttributes();
		}
		if (empty($moduleAttributes['uid'][0])) {
			return null;
		}
		return $moduleAttributes['uid'][0];
	}

	/**
	 * Runs the postmodify actions.
	 * 
	 * @see baseModule::postModifyActions()
	 *
	 * @param boolean $newAccount
	 * @param array $attributes LDAP attributes of this entry
	 * @return array array which contains status messages. Each entry is an array containing the status message parameters.
	 */
	public function postModifyActions($newAccount, $attributes) {
		$uid = $this->getUid();
		if (empty($uid)) {
			return array();
		}
		$ldapUser = $_SESSION['ldap']->decrypt_login();
		$ldapUser = $ldapUser[0];
		$messages = array();
		// calculate differences
		$toRem = $this->groupsOrig;
		$toAdd = $this->groups;
		$counter = sizeof($toRem);
		for ($i = 0; $i < $counter; $i++) {
			$group_orig = $toRem[$i];
			foreach ($toAdd as $k => $group) {
				if (($group_orig['dn'] == $group['dn'])
					&& ($group_orig['domain'] == $group['domain'])
					&& ($group_orig['host'] == $group['host'])) {
					if (!empty($this->uidOrig) && ($this->uidOrig != $uid)) {
						// uid changed, simply update uid in all net groups
					}
					else {
						// do not touch existing memberships
						unset($toRem[$i]);
						unset($toAdd[$k]);
					}
					break;
				}
			}
		}
		// group by DN
		$changes = array();
		foreach ($toAdd as $add) {
			$changes[$add['dn']]['add'][] = '(' . $add['host'] . ',' . $uid . ',' . $add['domain'] . ')';
		}
		foreach ($toRem as $del) {
			$delUid = empty($this->uidOrig) ? $uid : $this->uidOrig;
			$changes[$del['dn']]['del'][] = '(' . $del['host'] . ',' . $delUid . ',' . $del['domain'] . ')';
		}
		// update groups
		foreach ($changes as $dn => $changeSet) {
			$current = ldapGetDN($dn, array('nisnetgrouptriple'));
			if (empty($current)) {
				$messages[] = array('ERROR', sprintf(_('Was unable to modify attributes of DN: %s.'), $dn));
				continue;
			}
			$triples = empty($current['nisnetgrouptriple']) ? array() : $current['nisnetgrouptriple'];
			if (!empty($changeSet['del'])) {
				$triples = array_delete($changeSet['del'], $triples);
			}
			if (!empty($changeSet['add'])) {
				$triples = array_merge($changeSet['add'], $triples);
			}
			$triples = array_values(array_unique($triples));
			$attributes = array(
				'nisnetgrouptriple' => $triples
			);
			$success = @ldap_mod_replace($_SESSION['ldap']->server(), $dn, $attributes);
			if (!$success) {
				logNewMessage(LOG_ERR, '[' . $ldapUser .'] Unable to modify attributes of DN: ' . $dn . ' (' . ldap_error($_SESSION['ldap']->server()) . ').');
				$messages[] = array('ERROR', sprintf(_('Was unable to modify attributes of DN: %s.'), $dn), getDefaultLDAPErrorString($_SESSION['ldap']->server()));
			}
		}
		return $messages;
	}
	
	/**
	* Additional LDAP operations on delete.
	*
	* @return List of LDAP operations, same as for save_attributes()
	*/
	function delete_attributes() {
		$uid = $this->getUid();
		if (empty($uid)) {
			return array();
		}
		$ldapUser = $_SESSION['ldap']->decrypt_login();
		$ldapUser = $ldapUser[0];
		$return = array();
		// remove from NIS netgroups
		$changes = array();
		foreach ($this->groups as $group) {
			$changes[$group['dn']][] = '(' . $group['host'] . ',' . $uid . ',' . $group['domain'] . ')';
		}
		foreach ($changes as $dn => $changeSet) {
			$current = ldapGetDN($dn, array('nisnetgrouptriple'));
			if (empty($current)) {
				$messages[] = array('ERROR', sprintf(_('Was unable to modify attributes of DN: %s.'), $dn));
				continue;
			}
			$triples = empty($current['nisnetgrouptriple']) ? array() : $current['nisnetgrouptriple'];
			$triples = array_delete($changeSet, $triples);
			$triples = array_values(array_unique($triples));
			$attributes = array(
				'nisnetgrouptriple' => $triples
			);
			$success = @ldap_mod_replace($_SESSION['ldap']->server(), $dn, $attributes);
			if (!$success) {
				logNewMessage(LOG_ERR, '[' . $ldapUser .'] Unable to modify attributes of DN: ' . $dn . ' (' . ldap_error($_SESSION['ldap']->server()) . ').');
				$messages[] = array('ERROR', sprintf(_('Was unable to modify attributes of DN: %s.'), $dn), getDefaultLDAPErrorString($_SESSION['ldap']->server()));
			}
		}
		return $return;
	}

	/**
	* Returns a list of elements for the account profiles.
	*
	* @return profile elements
	*/
	function get_profileOptions() {
		$groups = $this->findGroups();
		$groupOptions = array('' => '');
		foreach ($groups as $group) {
			$groupOptions[$group['cn'][0]] = $group['cn'][0] . '#+#' . $group['dn'];
		}
		$return = new htmlTable();
		$return->addElement(new htmlOutputText(_('Group')));
		$return->addElement(new htmlOutputText(_('Host name')));
		$return->addElement(new htmlOutputText(_('Domain name')), true);
		for ($i = 0; $i < 5; $i++) {
			$select = new htmlSelect('nisNetGroupUser_group' . $i, $groupOptions, array(''));
			$select->setHasDescriptiveElements(true);
			$return->addElement($select);
			$return->addElement(new htmlInputField('nisNetGroupUser_host' . $i));
			$return->addElement(new htmlInputField('nisNetGroupUser_domain' . $i), true);
		}
		return $return;
	}

	/**
	* Loads the values of an account profile into internal variables.
	*
	* @param array $profile hash array with profile values (identifier => value)
	*/
	function load_profile($profile) {
		for ($i = 0; $i < 5; $i++) {
			if (!empty($profile['nisNetGroupUser_group' . $i][0])) {
				$parts = explode('#+#', $profile['nisNetGroupUser_group' . $i][0]);
				$this->groups[] = array(
					'name' => $parts[0],
					'dn' => $parts[1],
					'host' => $profile['nisNetGroupUser_host' . $i][0],
					'domain' => $profile['nisNetGroupUser_domain' . $i][0],
				);
			}
		}
		usort($this->groups, array($this, 'sortTriple'));
	}

	/**
	 * Returns a list of possible PDF entries for this account.
	 *
	 * @param array $pdfKeys list of PDF keys that are included in document
	 * @return list of PDF entries (array(<PDF key> => <PDF lines>))
	 */
	function get_pdfEntries($pdfKeys) {
		$return = array();
		$return[get_class($this) . '_memberships'][0] = '<block>'
			. '<tr><td width="25%" align=\"L\"><b>' . _('Group') . '</b></td>'
			. '<td width="25%" align=\"L\"><b>' . _('Host name') . '</b></td>'
			. '<td width="25%" align=\"L\"><b>' . _('Domain name') . '</b></td></tr></block>';
		foreach ($this->groups as $group) {
			$return[get_class($this) . '_memberships'][] = '<block><tr><td width="25%" align=\"L\">' . $group['name'] . '</td>'
				. '<td width="25%" align=\"L\">' . $group['host'] . ' </td>'
				. '<td width="25%" align=\"L\">' . $group['domain'] . ' </td></tr></block>';
		}
		return $return;
	}

	/**
	* In this function the LDAP account is built up.
	*
	* @param array $rawAccounts list of hash arrays (name => value) from user input
	* @param array $ids list of IDs for column position (e.g. "posixAccount_uid" => 5)
	* @param array $partialAccounts list of hash arrays (name => value) which are later added to LDAP
	* @param array $selectedModules list of selected account modules
	* @return array list of error messages if any
	*/
	function build_uploadAccounts($rawAccounts, $ids, &$partialAccounts, $selectedModules) {
		$errors = array();
		// get list of existing group of names
		$groups = $this->findGroups();
		$groupNames = array();
		foreach ($groups as $group) {
			$groupNames[] = $group['cn'][0];
		}
		// check input
		for ($i = 0; $i < sizeof($rawAccounts); $i++) {
			// group names
			if (!empty($rawAccounts[$i][$ids['nisNetGroupUser_memberships']])) {
				$triples = preg_split('/,[ ]*/', $rawAccounts[$i][$ids['nisNetGroupUser_memberships']]);
				foreach ($triples as $triple) {
					$parts = explode('#', $triple);
					if (!in_array($parts[0], $groupNames)) {
						$errors[] = array('ERROR', _('Unable to find group in LDAP.'), $parts[0]);
					}
				}
			}
		}
		return $errors;
	}

	/**
	* This function executes one post upload action.
	*
	* @param array $data array containing one account in each element
	* @param array $ids array(<column_name> => <column number>)
	* @param array $failed list of accounts which were not created successfully
	* @param array $temp variable to store temporary data between two post actions
	* @param array $accounts list of LDAP entries
	* @return array current status
	* <br> array (
	* <br>  'status' => 'finished' | 'inProgress'
	* <br>  'progress' => 0..100
	* <br>  'errors' => array (<array of parameters for StatusMessage>)
	* <br> )
	*/
	function doUploadPostActions(&$data, $ids, $failed, &$temp, &$accounts) {
		if (!checkIfWriteAccessIsAllowed($this->get_scope())) {
			die();
		}
		// on first call generate list of LDAP operations
		if (!isset($temp['counter'])) {
			$temp['groups'] = array();
			$temp['counter'] = 0;
			// get list of existing groups
			$groupList = $this->findGroups();
			$groupMap = array();
			foreach ($groupList as $group) {
				$groupMap[$group['cn'][0]] = $group['dn'];
			}
			for ($i = 0; $i < sizeof($data); $i++) {
				if (in_array($i, $failed)) continue; // ignore failed accounts
				if (empty($accounts[$i]['uid'])) {
					continue;
				}
				$uid = $accounts[$i]['uid'];
				if (!empty($data[$i][$ids['nisNetGroupUser_memberships']])) {
					$triples = preg_split('/,[ ]*/', $data[$i][$ids['nisNetGroupUser_memberships']]);
					foreach ($triples as $triple) {
						$parts = explode('#', $triple);
						$group = $parts[0];
						$host = empty($parts[1]) ? '' : $parts[1];
						$domain = empty($parts[2]) ? '' : $parts[2];
						$temp['groups'][$groupMap[$group]][] = '(' . $host . ',' . $uid . ',' . $domain . ')';
					}
				}
			}
			$temp['groupDNs'] = array_keys($temp['groups']);
			return array(
				'status' => 'inProgress',
				'progress' => 0,
				'errors' => array()
			);
		}
		// add users to groups
		elseif ($temp['counter'] < sizeof($temp['groupDNs'])) {
			$errors = array();
			$dn = $temp['groupDNs'][$temp['counter']];
			$current = ldapGetDN($dn, array('nisnetgrouptriple'));
			if (empty($current)) {
				$errors[] = array('ERROR', sprintf(_('Was unable to modify attributes of DN: %s.'), $dn));
				continue;
			}
			$triples = empty($current['nisnetgrouptriple']) ? array() : $current['nisnetgrouptriple'];
			$triples = array_merge($temp['groups'][$dn], $triples);
			$triples = array_values(array_unique($triples));
			$attributes = array(
				'nisnetgrouptriple' => $triples
			);
			$success = @ldap_mod_replace($_SESSION['ldap']->server(), $dn, $attributes);
			if (!$success) {
				$errors[] = array(
					"ERROR",
					_("LAM was unable to modify group memberships for group: %s"),
					getDefaultLDAPErrorString($_SESSION['ldap']->server()),
					array($dn)
				);
			}
			$temp['counter']++;
			return array (
				'status' => 'inProgress',
				'progress' => ($temp['counter'] * 100) / sizeof($temp['groupDNs']),
				'errors' => $errors
			);
		}
		// all modifications are done
		else {
			return array (
				'status' => 'finished',
				'progress' => 100,
				'errors' => array()
			);
		}
	}

	/**
	 * Finds all existing LDAP NIS net groups.
	 *
	 * @return array groups array(array(cn => array(), dn => '', nisnetgrouptriple => array()))
	 */
	private function findGroups() {
		if ($this->groupCache != null) {
			return $this->groupCache;
		}
		$return = array();
		$types = array('netgroup');
		$typeSettings = $_SESSION['config']->get_typeSettings();
		if (sizeof($types) > 0) {
			foreach ($types as $type) {
				$filter = '(objectClass=nisNetgroup)';
				if (!empty($typeSettings['filter_' . $type])) {
					$typeFilter = $typeSettings['filter_' . $type];
					if (strpos($typeFilter, '(') !== 0) {
						$typeFilter = '(' . $typeFilter . ')';
					}
					$filter = '(&' . $filter . $typeFilter . ')';
				}			
				$results = searchLDAPByFilter($filter, array('cn', 'dn', 'nisnetgrouptriple'), array($type));
				for ($i = 0; $i < sizeof($results); $i++) {
					if (isset($results[$i]['cn'][0]) && isset($results[$i]['dn'])) {
						$return[] = $results[$i];
					}
				}
			}
		}
		$this->groupCache = $return;
		return $return;
	}
	
	/**
	 * Sorts NIS netgroup triples by group, host and domain.
	 * 
	 * @param array $first first array
	 * @param array $second second array
	 */
	private function sortTriple($first, $second) {
		if ($first['name'] != $second['name']) {
			return strnatcasecmp($first['name'], $second['name']);
		}
		elseif ($first['host'] != $second['host']) {
			return strnatcasecmp($first['host'], $second['host']);
		}
		return strnatcasecmp($first['domain'], $second['domain']);
	}
	
}

?>