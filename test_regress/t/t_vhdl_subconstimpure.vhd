-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A record constant a subprogram declares, given what an impure function
-- returns: VUnit's get_running_test_case, "constant running_test_case :
-- string_ptr_t := to_string_ptr(get(runner.p_data, running_test_case_idx))"
-- and then "if running_test_case = null_string_ptr". Worked out once, at
-- elaboration, the pointer was never null, and the test was dropped as a
-- branch nothing reaches. A subprogram's constant is given its value on
-- each call.
package t_vhdl_subconstimpure_pkg is
  type ptr_t is record
    ref : integer;
  end record;
  constant null_ptr : ptr_t := (ref => -1);
  impure function get(idx : natural) return integer;
  procedure set(idx : natural; value : integer);
  impure function describe return string;
  function to_ptr(value : integer) return ptr_t;
  impure function describe2 return string;
end package;

package body t_vhdl_subconstimpure_pkg is
  type store_t is protected
    impure function get(idx : natural) return integer;
    procedure set(idx : natural; value : integer);
  end protected;

  type store_t is protected body
    type int_array is array (0 to 9) of integer;
    variable data : int_array := (others => 5);
    impure function get(idx : natural) return integer is
    begin
      return data(idx);
    end function;
    procedure set(idx : natural; value : integer) is
    begin
      data(idx) := value;
    end procedure;
  end protected body;

  shared variable store : store_t;

  impure function get(idx : natural) return integer is
  begin
    return store.get(idx);
  end function;

  procedure set(idx : natural; value : integer) is
  begin
    store.set(idx, value);
  end procedure;

  impure function describe return string is
    constant p : ptr_t := (ref => get(8));
  begin
    if p = null_ptr then
      return "none";
    end if;
    return "some";
  end function;

  function to_ptr(value : integer) return ptr_t is
  begin
    return (ref => value);
  end function;

  -- The value is one call returning the record, as VUnit writes it
  impure function describe2 return string is
    constant p : ptr_t := to_ptr(get(8));
  begin
    if p = null_ptr then
      return "none";
    end if;
    return "some";
  end function;
end package body;

use work.t_vhdl_subconstimpure_pkg.all;

entity t_vhdl_subconstimpure is
end entity;

architecture a of t_vhdl_subconstimpure is
begin
  process
  begin
    assert describe = "some" and describe2 = "some" severity failure;
    set(8, -1);
    assert describe = "none" report describe severity failure;
    assert describe2 = "none" report describe2 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
