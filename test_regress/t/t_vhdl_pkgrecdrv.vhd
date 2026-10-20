-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A record whose elements functions of the design resolve, declared in a
-- package -- an instance of a generic one, as OSVVM's Wishbone harness
-- declares its transaction records, or a plain one -- and handed to the
-- inout ports of two instances. Each instance is a source of it (14.7.3.2),
-- but each wrote the whole signal, the last one winning: the model's Ack
-- went back to -1 and it took a transaction nobody had requested. Two
-- package instances' signals of one name are resolved in one module.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_pkgrecdrv_pkg is
  function resolved_max(s : integer_vector) return integer;
  subtype RdyType is resolved_max integer range 0 to integer'high;
  subtype AckType is resolved_max integer range -1 to integer'high;
  type op_t is (NOT_DRIVEN, WRITE_OP, READ_OP);
  type op_vector is array (natural range <>) of op_t;
  function resolved_max(s : op_vector) return op_t;
  subtype ResOp is resolved_max op_t;
  type rec_t is record
    Rdy : RdyType;
    Ack : AckType;
    Operation : ResOp;
    Param : resolved_max integer;
  end record;
  function Increment(i : integer) return integer;
  procedure RequestTransaction(signal Rdy : inout RdyType; signal Ack : in AckType);
  procedure WaitForClock(signal Rec : inout rec_t; n : integer);
  procedure WaitForTransaction(signal Clk : in std_logic; signal Rdy : in RdyType;
                               signal Ack : inout AckType);
end package;
package body t_vhdl_pkgrecdrv_pkg is
  function resolved_max(s : integer_vector) return integer is
    variable r : integer := integer'low;
  begin
    for i in s'range loop
      if s(i) > r then r := s(i); end if;
    end loop;
    return r;
  end function;
  function resolved_max(s : op_vector) return op_t is
    variable r : op_t := NOT_DRIVEN;
  begin
    for i in s'range loop
      if s(i) > r then r := s(i); end if;
    end loop;
    return r;
  end function;
  function Increment(i : integer) return integer is
  begin
    return i + 1;
  end function;
  procedure RequestTransaction(signal Rdy : inout RdyType; signal Ack : in AckType) is
  begin
    Rdy <= Increment(Rdy);
    wait for 0 ns;
    wait until Rdy = Ack;
  end procedure;
  procedure WaitForClock(signal Rec : inout rec_t; n : integer) is
  begin
    Rec.Operation <= WRITE_OP;
    Rec.Param <= n;
    RequestTransaction(Rdy => Rec.Rdy, Ack => Rec.Ack);
  end procedure;
  procedure WaitForTransaction(signal Clk : in std_logic; signal Rdy : in RdyType;
                               signal Ack : inout AckType) is
  begin
    Ack <= Increment(Ack);
    wait until Ack /= Rdy;
    if not rising_edge(Clk) then
      wait until rising_edge(Clk);
    end if;
  end procedure;
end package body;

use work.t_vhdl_pkgrecdrv_pkg.all;
package t_vhdl_pkgrecdrv_sigs is
  generic (W : natural);
  signal TransRec : rec_t;
end package;

use work.t_vhdl_pkgrecdrv_pkg.all;
package t_vhdl_pkgrecdrv_plain is
  signal TransRec : rec_t;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgrecdrv_pkg.all;
entity t_vhdl_pkgrecdrv_mdl is
  port (Clk : in std_logic; TransRec : inout rec_t);
end;
architecture a of t_vhdl_pkgrecdrv_mdl is
begin
  process
  begin
    wait for 0 ns;
    TransRec.Param <= 7;
    loop
      WaitForTransaction(Clk => Clk, Rdy => TransRec.Rdy, Ack => TransRec.Ack);
      report "model got " & op_t'image(TransRec.Operation) & " at " & time'image(now)
        & " Rdy=" & integer'image(TransRec.Rdy) & " Ack=" & integer'image(TransRec.Ack);
      assert TransRec.Operation = WRITE_OP and now = 55 ns
        report "a transaction nobody requested" severity failure;
    end loop;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgrecdrv_pkg.all;
entity t_vhdl_pkgrecdrv_ctl is
  port (Rec : inout rec_t);
end;
architecture a of t_vhdl_pkgrecdrv_ctl is
begin
  process
  begin
    wait for 50 ns;
    WaitForClock(Rec, 2);
    report "ctl done at " & time'image(now);
    assert now = 55 ns severity failure;
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgrecdrv_pkg.all;
entity t_vhdl_pkgrecdrv is end;
architecture t of t_vhdl_pkgrecdrv is
  signal Clk : std_logic := '0';
  package Manager is new work.t_vhdl_pkgrecdrv_sigs generic map (W => 32);
  package Subordinate is new work.t_vhdl_pkgrecdrv_sigs generic map (W => 32);
begin
  Clk <= not Clk after 5 ns when now < 200 ns;
  m : entity work.t_vhdl_pkgrecdrv_mdl port map (Clk => Clk, TransRec => Manager.TransRec);
  c : entity work.t_vhdl_pkgrecdrv_ctl port map (Rec => Manager.TransRec);
  sm : entity work.t_vhdl_pkgrecdrv_mdl port map (Clk => Clk, TransRec => Subordinate.TransRec);
  sc : entity work.t_vhdl_pkgrecdrv_ctl port map (Rec => Subordinate.TransRec);
  pm : entity work.t_vhdl_pkgrecdrv_mdl port map (Clk => Clk, TransRec => work.t_vhdl_pkgrecdrv_plain.TransRec);
  pc : entity work.t_vhdl_pkgrecdrv_ctl port map (Rec => work.t_vhdl_pkgrecdrv_plain.TransRec);
  process
  begin
    wait for 100 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
