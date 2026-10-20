-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An inout port of a record whose elements a function of the design resolves,
-- driven only through a procedure given one of its elements -- OSVVM's
-- WaitForTransaction(Ack => TransRec.Ack) -- with no assignment of its own.
-- The analysis counted the element as driven, not the record, so the port had
-- no driver and was lowered as an input ("Assigning to input/const
-- variable").

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_procportdrv_pkg is
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
package body t_vhdl_procportdrv_pkg is
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

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_procportdrv_pkg.all;
entity t_vhdl_procportdrv_mdl is
  port (Clk : in std_logic; TransRec : inout rec_t);
end;
architecture a of t_vhdl_procportdrv_mdl is
begin
  process
  begin
    wait for 0 ns;
    loop
      WaitForTransaction(Clk => Clk, Rdy => TransRec.Rdy, Ack => TransRec.Ack);
      report "model got " & op_t'image(TransRec.Operation) & " at " & time'image(now)
        & " Rdy=" & integer'image(TransRec.Rdy) & " Ack=" & integer'image(TransRec.Ack);
    end loop;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_procportdrv_pkg.all;
entity t_vhdl_procportdrv_ctl is
  port (Rec : inout rec_t);
end;
architecture a of t_vhdl_procportdrv_ctl is
begin
  process
  begin
    wait for 50 ns;
    WaitForClock(Rec, 2);
    report "ctl done at " & time'image(now);
    assert now = 55 ns severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_procportdrv_pkg.all;
entity t_vhdl_procportdrv is end;
architecture t of t_vhdl_procportdrv is
  signal Clk : std_logic := '0';
  signal TransRec : rec_t;
begin
  Clk <= not Clk after 5 ns when now < 200 ns;
  m : entity work.t_vhdl_procportdrv_mdl port map (Clk => Clk, TransRec => TransRec);
  c : entity work.t_vhdl_procportdrv_ctl port map (Rec => TransRec);
end;
