-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An alias with a subtype of its own, whose length a generic settles, over a
-- slice of a value carried with its bounds: OSVVM's AXI stream receiver
-- (AxiStreamReceiver.vhd) writes "alias ID : std_logic_vector(ID_LEN - 1
-- downto 0) is aParam(...)", aParam an alias of a parameter of any length.
-- The alias is the slice in its own shape (6.6.2); it was handed on as the
-- carried slice, which a copy of to_hstring for four elements cannot take.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aliasownshape_rx is
  generic (ID_LEN : natural := 4; DEST_LEN : natural := 2; USER_LEN : natural := 3;
           EXPECT : string := "");
end;
architecture a of t_vhdl_aliasownshape_rx is
  constant PARAM_LENGTH : integer := ID_LEN + DEST_LEN + USER_LEN + 1;
begin
  process
    function param_to_string(Param : std_logic_vector) return string is
      alias aParam : std_logic_vector(Param'length - 1 downto 0) is Param;
      alias ID : std_logic_vector(ID_LEN - 1 downto 0)
        is aParam(PARAM_LENGTH - 1 downto PARAM_LENGTH - ID_LEN);
      alias Dest : std_logic_vector(DEST_LEN - 1 downto 0)
        is aParam(DEST_LEN + USER_LEN downto USER_LEN + 1);
      alias User : std_logic_vector(USER_LEN - 1 downto 0) is aParam(USER_LEN downto 1);
      alias Last : std_logic is Param(0);
    begin
      return "TID: " & to_hstring(ID) & " TDest: " & to_hstring(Dest)
             & " TUser: " & to_hstring(User) & " TLast: " & to_string(Last)
             & " ID'left: " & integer'image(ID'left);
    end function;
    variable Param : std_logic_vector(PARAM_LENGTH - 1 downto 0) := (others => '1');
  begin
    Param(PARAM_LENGTH - 2 downto PARAM_LENGTH - 4) := "010";
    report param_to_string(Param);
    assert param_to_string(Param) = EXPECT
      report "got " & param_to_string(Param) severity failure;
    wait;
  end process;
end;

entity t_vhdl_aliasownshape is end;
architecture t of t_vhdl_aliasownshape is
begin
  u1 : entity work.t_vhdl_aliasownshape_rx
    generic map (EXPECT => "TID: A TDest: 3 TUser: 7 TLast: 1 ID'left: 3");
  u2 : entity work.t_vhdl_aliasownshape_rx
    generic map (ID_LEN => 8, DEST_LEN => 1, USER_LEN => 2,
                 EXPECT => "TID: AF TDest: 1 TUser: 3 TLast: 1 ID'left: 7");
  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
