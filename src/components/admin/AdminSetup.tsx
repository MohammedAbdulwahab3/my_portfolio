import { useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { useAuth } from "@/contexts/AuthContext";
import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { Alert, AlertDescription } from "@/components/ui/alert";
import { toast } from "sonner";
import ForceAdminSetup from "./ForceAdminSetup";
import TemporaryBypass from "./TemporaryBypass";

const AdminSetup = () => {
  const [loading, setLoading] = useState(false);
  const { user, isAdmin } = useAuth();

  const setupAdminRole = async () => {
    if (!user) {
      toast.error("You must be logged in to set up admin role");
      return;
    }

    setLoading(true);
    try {
      // Try to insert admin role for current user
      const { error } = await supabase
        .from("user_roles")
        .insert([
          {
            user_id: user.id,
            role: "admin"
          }
        ]);

      if (error) {
        console.error("Error adding admin role:", error);
        
        // Check if it's a policy violation (user already has role or RLS issue)
        if (error.code === '23505') {
          toast.success("You already have admin role! Please refresh the page.");
        } else if (error.code === '42501' || error.message.includes('policy')) {
          toast.error("Unable to auto-assign admin role. This might be the first setup. Please contact support or check the console for manual setup instructions.");
          console.log("Manual setup required. Run this SQL in your Supabase dashboard:");
          console.log(`INSERT INTO public.user_roles (user_id, role) VALUES ('${user.id}', 'admin');`);
        } else {
          toast.error("Failed to add admin role: " + error.message);
        }
      } else {
        toast.success("Admin role added successfully! Please refresh the page.");
        // Force a page refresh to update the auth context
        setTimeout(() => window.location.reload(), 1500);
      }
    } catch (error) {
      console.error("Error:", error);
      toast.error("An error occurred while setting up admin role");
    } finally {
      setLoading(false);
    }
  };

  if (isAdmin) {
    return (
      <Card className="p-6">
        <h2 className="text-xl font-bold mb-4">Admin Setup Complete</h2>
        <p className="text-green-600">✅ You have admin privileges!</p>
      </Card>
    );
  }

  return (
    <Card className="p-6">
      <h2 className="text-xl font-bold mb-4">Admin Setup Required</h2>
      
      <Alert className="mb-4">
        <AlertDescription>
          You need admin privileges to manage testimonials, blog posts, and other content. 
          Click the button below to set up admin access.
        </AlertDescription>
      </Alert>

      <div className="space-y-4">
        <p className="text-sm text-muted-foreground">
          Current user: {user?.email || "Not logged in"}
        </p>
        
        <Button 
          onClick={setupAdminRole} 
          disabled={loading || !user}
          className="w-full"
        >
          {loading ? "Setting up..." : "Setup Admin Role"}
        </Button>

        <div className="text-xs text-muted-foreground space-y-2">
          <p><strong>Alternative manual setup:</strong></p>
          <p>1. Access your Supabase dashboard</p>
          <p>2. Go to SQL Editor</p>
          <p>3. Run this SQL command:</p>
          <div className="bg-muted p-2 rounded font-mono text-xs">
            INSERT INTO public.user_roles (user_id, role) VALUES ('{user?.id}', 'admin');
          </div>
          <Button 
            variant="outline" 
            size="sm" 
            onClick={() => {
              navigator.clipboard.writeText(`INSERT INTO public.user_roles (user_id, role) VALUES ('${user?.id}', 'admin');`);
              toast.success("SQL command copied to clipboard!");
            }}
            className="mt-2"
          >
            Copy SQL Command
          </Button>
        </div>
      </div>

      <div className="mt-6">
        <ForceAdminSetup />
      </div>

      <div className="mt-6">
        <TemporaryBypass />
      </div>
    </Card>
  );
};

export default AdminSetup;
