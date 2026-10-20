-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- INOUT ports of subtypes that functions of the design's own resolve, between
-- instances: a scalar resolved as a whole, and a record each of whose
-- subelements is resolved on its own, with no function for the record -- the
-- handshake OSVVM's transaction records make between a test sequencer and a
-- verification component. Each instance reads what the drivers of every
-- instance resolve to, element by element (6.3, 14.7.3.2, 14.7.3.3), whether
-- or not the instances are inlined. A process driving one element of the
-- record drives only that element; a procedure handed the whole record drives
-- all of it.

package t_vhdl_recleafres_pkg is
  function imax(s : integer_vector) return integer;
  subtype int_max is imax integer range -1 to integer'high;
  subtype ack_t is imax integer range -1 to integer'high;
  subtype rdy_t is imax integer range 0 to integer'high;
  function bmax(s : bit_vector) return bit;
  subtype bit_max is bmax bit;
  type bmax_vec is array (natural range <>) of bit_max;
  type id_t is record
    id : rdy_t;
  end record;
  type rec_t is record
    rdy  : rdy_t;
    ack  : ack_t;
    data : bmax_vec;
    sb   : id_t;
  end record;
  subtype rec8_t is rec_t(data(7 downto 0));
  procedure req(signal r : inout rec8_t; d : bit_vector(7 downto 0));
  procedure req_inner(signal r : inout rec8_t; d : bit_vector(7 downto 0));
  procedure serve(signal r : inout rec8_t; d : out bit_vector(7 downto 0));
  procedure wait_req(signal rdy : in rdy_t; signal ack : inout ack_t);
  procedure request(signal rdy : inout rdy_t; signal ack : in ack_t);
end package;

package body t_vhdl_recleafres_pkg is
  function imax(s : integer_vector) return integer is
    variable m : integer := integer'low;
  begin
    for i in s'range loop
      if s(i) > m then m := s(i); end if;
    end loop;
    return m;
  end function;
  function bmax(s : bit_vector) return bit is
  begin
    for i in s'range loop
      if s(i) = '1' then return '1'; end if;
    end loop;
    return '0';
  end function;
  procedure req_inner(signal r : inout rec8_t; d : bit_vector(7 downto 0)) is
  begin
    for i in d'range loop
      r.data(i) <= d(i);
    end loop;
    -- The elements of its own formal handed on, as OSVVM's LocalSend hands them to
    -- RequestTransaction
    request(r.rdy, r.ack);
  end procedure;
  -- The whole of its formal handed on, as OSVVM's Send hands it to LocalSend
  procedure req(signal r : inout rec8_t; d : bit_vector(7 downto 0)) is
  begin
    req_inner(r, d);
  end procedure;
  procedure serve(signal r : inout rec8_t; d : out bit_vector(7 downto 0)) is
  begin
    r.ack <= r.ack + 1;
    wait until r.ack /= r.rdy;
    for i in d'range loop
      d(i) := r.data(i);
    end loop;
  end procedure;
  procedure request(signal rdy : inout rdy_t; signal ack : in ack_t) is
  begin
    rdy <= rdy + 1;
    wait for 0 ns;
    wait until rdy = ack;
  end procedure;
  -- Handed two elements of the record, as OSVVM's WaitForTransaction is
  procedure wait_req(signal rdy : in rdy_t; signal ack : inout ack_t) is
  begin
    ack <= ack + 1;
    wait until ack /= rdy;
  end procedure;
end package body;

use work.t_vhdl_recleafres_pkg.all;
entity t_vhdl_recleafres_side is
  generic (id : integer);
  port (x : inout int_max);
end entity;
architecture a of t_vhdl_recleafres_side is
begin
  process
  begin
    assert x = -1 severity failure;
    wait for id * 10 ns;
    x <= 12 - id * 4;
    wait for 1 ns;
    -- side 1 drives 8, side 2 drives 4: the maximum is 8 once both have driven
    assert x = 8 severity failure;
    wait;
  end process;
end architecture;

use work.t_vhdl_recleafres_pkg.all;
entity t_vhdl_recleafres_vc is
  port (r : inout rec8_t);
end entity;
architecture a of t_vhdl_recleafres_vc is
begin
  process
    variable d : bit_vector(7 downto 0);
  begin
    assert r.ack = -1 and r.rdy = 0 severity failure;
    r.sb.id <= 5;
    serve(r, d);
    assert d = "10100101" and r.rdy = 1 and now = 10 ns severity failure;
    wait_req(r.rdy, r.ack);
    for i in d'range loop
      d(i) := r.data(i);
    end loop;
    -- Only the sequencer drives the data, so its "00001111" is the data
    assert d = "00001111" and r.rdy = 2 and now = 20 ns severity failure;
    -- Acknowledges the second request, and waits for a third that never comes
    wait_req(r.rdy, r.ack);
    wait;
  end process;
end architecture;

use work.t_vhdl_recleafres_pkg.all;
entity t_vhdl_recleafres_ctl is
  port (r : inout rec8_t);
end entity;
architecture a of t_vhdl_recleafres_ctl is
begin
  process
  begin
    assert r.ack = -1 and r.sb.id = 0 severity failure;
    wait for 10 ns;
    req(r, "10100101");
    assert now = 10 ns and r.sb.id = 5 severity failure;
    wait for 10 ns;
    req(r, "00001111");
    assert now = 20 ns and r.ack = 2 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;

use work.t_vhdl_recleafres_pkg.all;
entity t_vhdl_recleafres is
end entity;
architecture t of t_vhdl_recleafres is
  signal x : int_max;
  signal tr : rec8_t;
begin
  u1 : entity work.t_vhdl_recleafres_side generic map (1) port map (x);
  u2 : entity work.t_vhdl_recleafres_side generic map (2) port map (x);
  u3 : entity work.t_vhdl_recleafres_vc port map (tr);
  u4 : entity work.t_vhdl_recleafres_ctl port map (r => tr);
  process
  begin
    wait for 15 ns;
    assert tr.rdy = 1 and tr.ack = 1 and tr.data = "10100101" and tr.sb.id = 5
      severity failure;
    wait for 15 ns;
    assert x = 8 severity failure;
    wait;
  end process;
end architecture;
