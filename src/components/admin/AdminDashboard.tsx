import { useAuth } from "@/contexts/AuthContext";
import AdminSetup from "./AdminSetup";
import EnhancedBlogManagement from "./EnhancedBlogManagement";
import TestimonialsManagement from "./TestimonialsManagement";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { Card } from "@/components/ui/card";

const AdminDashboard = () => {
  const { user, isAdmin, loading } = useAuth();

  if (loading) {
    return (
      <div className="flex items-center justify-center min-h-screen">
        <div className="text-center">
          <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-primary mx-auto mb-4"></div>
          <p>Loading...</p>
        </div>
      </div>
    );
  }

  if (!user) {
    return (
      <Card className="p-6 max-w-md mx-auto mt-8">
        <h2 className="text-xl font-bold mb-4">Authentication Required</h2>
        <p>Please sign in to access the admin dashboard.</p>
      </Card>
    );
  }

  if (!isAdmin) {
    return (
      <div className="max-w-2xl mx-auto mt-8">
        <AdminSetup />
      </div>
    );
  }

  return (
    <div className="container mx-auto py-8">
      <div className="mb-8">
        <h1 className="text-3xl font-bold">Admin Dashboard</h1>
        <p className="text-muted-foreground">Welcome back, {user.email}</p>
      </div>

      <Tabs defaultValue="blog" className="space-y-6">
        <TabsList className="grid w-full grid-cols-2">
          <TabsTrigger value="blog">Blog Management</TabsTrigger>
          <TabsTrigger value="testimonials">Testimonials</TabsTrigger>
        </TabsList>
        
        <TabsContent value="blog" className="space-y-6">
          <EnhancedBlogManagement />
        </TabsContent>
        
        <TabsContent value="testimonials" className="space-y-6">
          <TestimonialsManagement />
        </TabsContent>
      </Tabs>
    </div>
  );
};

export default AdminDashboard;
